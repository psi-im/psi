/*
 * psiaccount.cpp - handles a Psi account
 * Copyright (C) 2001-2005  Justin Karneges
 * Copyright (C) 2020  Boris Pek <tehnick-8@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "pgputil.h"

#include "gpgprocess.h"
#include "showtextdlg.h"

#include <QDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QtCore>

PGPUtil *PGPUtil::m_instance = nullptr;

PGPUtil::PGPUtil()
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(deleteLater()));
}

PGPUtil &PGPUtil::instance()
{
    if (!m_instance) {
        m_instance = new PGPUtil();
    }
    return *m_instance;
}

bool PGPUtil::pgpAvailable()
{
    QString message;
    GpgProcess gpg;
    return gpg.info(message);
}

QString PGPUtil::stripHeaderFooter(const QString &str)
{
    QString s;
    if (str.isEmpty()) {
        qWarning("pgputil.cpp: Empty PGP message");
        return "";
    }
    if (str.at(0) != '-')
        return str;
    QStringList                lines = str.split('\n');
    QStringList::ConstIterator it    = lines.begin();
    // skip the first line
    ++it;
    if (it == lines.end())
        return str;

    // skip the header
    for (; it != lines.end(); ++it) {
        if ((*it).isEmpty())
            break;
    }
    if (it == lines.end())
        return str;
    ++it;
    if (it == lines.end())
        return str;

    bool first = true;
    for (; it != lines.end(); ++it) {
        if ((*it).at(0) == '-')
            break;
        if (!first)
            s += '\n';
        s += (*it);
        first = false;
    }

    return s;
}

QString PGPUtil::addHeaderFooter(const QString &str, int type)
{
    QString stype;
    if (type == 0)
        stype = "MESSAGE";
    else
        stype = "SIGNATURE";

    QString s;
    s += QString("-----BEGIN PGP %1-----\n").arg(stype);
    s += "Version: PGP\n";
    s += '\n';
    s += str + '\n';
    s += QString("-----END PGP %1-----\n").arg(stype);
    return s;
}

bool PGPUtil::equals(const QString &k1, const QString &k2)
{
    if (k1.isEmpty()) {
        return k2.isEmpty();
    } else if (k2.isEmpty()) {
        return false;
    } else {
        return (k1 == k2);
    }
}

QString PGPUtil::getKeyOwnerName(const QString &key)
{
    if (key.isEmpty())
        return QString();

    const QStringList &&arguments = { "--list-public-keys", "--with-colons", "--fixed-list-mode", "0x" + key };

    GpgProcess gpg;
    gpg.start(arguments);
    gpg.waitForFinished();

    if (!gpg.success())
        return QString();

    const QString &&rawData = QString::fromUtf8(gpg.readAllStandardOutput());
    if (rawData.isEmpty())
        return QString();

    QString name;
    const QStringList &&stringsList = rawData.split("\n");
    for (const QString &line : stringsList) {
        const QString &&type = line.section(':', 0, 0);
        if (type == "uid") {
            name = line.section(':', 9, 9); // Name
            break;
        }
    }
    return name;
}

QString PGPUtil::getPublicKeyData(const QString &key)
{
    if (key.isEmpty())
        return QString();

    const QStringList &&arguments = { "--armor", "--export", "0x" + key };

    GpgProcess gpg;
    gpg.start(arguments);
    gpg.waitForFinished();

    if (!gpg.success())
        return QString();

#ifdef Q_OS_WIN
    QString keyData = QString::fromUtf8(gpg.readAllStandardOutput());
    keyData.replace("\r","");
#else
    const QString &&keyData = QString::fromUtf8(gpg.readAllStandardOutput());
#endif
    return keyData;
}

QString PGPUtil::getFingerprint(const QString &key)
{
    const QStringList &&arguments = { "--with-colons", "--fingerprint", "0x" + key };

    GpgProcess gpg;
    gpg.start(arguments);
    gpg.waitForFinished();

    QString             fingerprint;
    const QString &&    out   = QString::fromUtf8(gpg.readAllStandardOutput());
    const QStringList &&lines = out.split("\n");
    for (const QString &line : lines) {
        const QString &&type = line.section(':', 0, 0);
        if (type == "fpr") {
            fingerprint = line.section(':', 9, 9);
            break;
        }
    }

    if (fingerprint.size() == 40) {
        for (int k = fingerprint.size() - 4; k >= 3; k -= 4) {
            fingerprint.insert(k, ' ');
        }
        fingerprint.insert(24, ' ');
        return fingerprint;
    }

    return QString();
}

QString PGPUtil::chooseKey(PGPKeyDlg::Type type, const QString &key, const QString &title)
{
    PGPKeyDlg d(type, key, nullptr);
    d.setWindowTitle(title);
    if (d.exec() == QDialog::Accepted) {
        return d.keyId();
    }
    return QString();
}

PGPUtil::SecureMessageSignature PGPUtil::parseSecureMessageSignature(const QString &stdOutString)
{
    SecureMessageSignature out;
    const QStringList &&strings = stdOutString.split("\n");
    for (const QString &line : strings) {
        const QString &&type = line.section(' ', 1, 1);
        if (type == QStringLiteral("GOODSIG")) {
            out.identityResult = SecureMessageSignature::Valid;
            out.publicKeyId = line.section(' ', 2, 2);
            if (out.publicKeyId.size() > 16) {
                out.publicKeyId = out.publicKeyId.right(16);
            }
            out.userName = line.section(' ', 3);
        } else if (type == QStringLiteral("VALIDSIG")) {
            out.sigTimestamp = line.section(' ', 4, 4).toLongLong();
            if (!out.userName.isEmpty()) {
                break;
            }
        } if (type == QStringLiteral("BADSIG")) {
            out.identityResult = SecureMessageSignature::InvalidSignature;
            out.publicKeyId = line.section(' ', 2, 2);
            if (out.publicKeyId.size() > 16) {
                out.publicKeyId = out.publicKeyId.right(16);
            }
            out.userName = line.section(' ', 3);
        } if (type == QStringLiteral("ERRSIG")) {
            out.identityResult = SecureMessageSignature::NoKey;
        }
    }
    if (out.publicKeyId.isEmpty()) {
        out.identityResult = SecureMessageSignature::NoKey;
    }
    return out;
}

void PGPUtil::showDiagnosticText(const QString &event, const QString &diagnostic)
{
    const QString &message = tr("There was an error trying to send the message encrypted.\nReason: %1.").arg(event);
    while (true) {
        QMessageBox  msgbox(QMessageBox::Critical, tr("Error"), message, QMessageBox::Ok, nullptr);
        QPushButton *diag = msgbox.addButton(tr("Diagnostics"), QMessageBox::HelpRole);
        msgbox.exec();
        if (msgbox.clickedButton() == diag) {
            ShowTextDlg *w = new ShowTextDlg(diagnostic, true, false, nullptr);
            w->setWindowTitle(tr("OpenPGP Diagnostic Text"));
            w->resize(560, 240);
            w->exec();

            continue;
        } else {
            break;
        }
    }
}
