/*
 * sslcertdlg.cpp
 * Copyright (C) 2003  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QtCrypto>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>

#include "common.h"
#include "iconwidget.h"
#include "certutil.h"
#include "sslcertdlg.h"


SSLCertDlg::SSLCertDlg(QWidget *parent)
:QDialog(parent)
{
	ui_.setupUi(this);
	setModal(true);
	setWindowTitle(CAP(caption()));

	connect(ui_.pb_close, SIGNAL(clicked()), SLOT(close()));
	ui_.pb_close->setDefault(true);
	ui_.pb_close->setFocus();
}

QString SSLCertDlg::makePropTable(const QString &heading, const QCA::CertificateInfo &list)
{
	QString str;
	str += "<tr><td><i>" + heading + "</i><br>";
	str += "<table>";
	str += makePropEntry(QCA::Organization, tr("Organization:"), list);
	str += makePropEntry(QCA::OrganizationalUnit, tr("Organizational unit:"), list);
	str += makePropEntry(QCA::Locality, tr("Locality:"), list);
	str += makePropEntry(QCA::State, tr("State:"), list);
	str += makePropEntry(QCA::Country, tr("Country:"), list);
	str += makePropEntry(QCA::CommonName, tr("Common name:"), list);
	str += makePropEntry(QCA::Email, tr("Email:"), list);
	str += "</table></td></tr>";
	return str;
}

void SSLCertDlg::setCert(const QCA::Certificate &cert, int result, QCA::Validity validity)
{
	if(cert.isNull())
		return;

	if(result == QCA::TLS::Valid) {
		ui_.lb_valid->setText(tr("The certificate is valid."));
		setLabelStatus(*ui_.lb_valid,true);
	}
	else {
		ui_.lb_valid->setText(tr("The certificate is NOT valid!") + "\n" + QString(tr("Reason: %1.")).arg(CertUtil::resultToString(result, validity)));
		setLabelStatus(*ui_.lb_valid,false);
	}

	QDateTime now = QDateTime::currentDateTime();
	QDateTime notBefore = cert.notValidBefore();
	QDateTime notAfter = cert.notValidAfter();
	ui_.lb_notBefore->setText(cert.notValidBefore().toString());
	setLabelStatus(*ui_.lb_notBefore, now > notBefore);
	ui_.lb_notAfter->setText(cert.notValidAfter().toString());
	setLabelStatus(*ui_.lb_notAfter, now < notAfter);

	ui_.lb_sn->setText(cert.serialNumber().toString());

	QString str;
	str += "<table>";
	str += makePropTable(tr("Subject Details:"), cert.subjectInfo());
	str += makePropTable(tr("Issuer Details:"), cert.issuerInfo());
	str += "</table>";
	ui_.tb_cert->setText(str);
}

void SSLCertDlg::showCert(const QCA::Certificate &certificate, int result, QCA::Validity validity)
{
	SSLCertDlg *w = new SSLCertDlg(0);
	w->setCert(certificate, result, validity);
	w->exec();
	delete w;
}

void SSLCertDlg::setLabelStatus(QLabel& l, bool ok)
{
	l.setPaletteForegroundColor(ok ? QColor("#2A993B") : QColor("#810000"));
}

QString SSLCertDlg::makePropEntry(QCA::CertificateInfoType var, const QString &name, const QCA::CertificateInfo &list)
{
	QString val;
	QList<QString> values = list.values(var);
	for (int i = 0; i < values.size(); ++i)
		val += values.at(i) + "<br>";

	if(val.isEmpty())
		return "";
	else
		return QString("<tr><td><nobr><b>") + name + "</b></nobr></td><td>" + val + "</td></tr>";
}
