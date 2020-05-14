/*
 * pgpkeydlg.h
 * Copyright (C) 2001-2009  Justin Karneges, Michail Pishchagin
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

#include "pgpkeydlg.h"

#include "common.h"
#include "gpgprocess.h"
#include "showtextdlg.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QString>

class KeyViewItem : public QStandardItem {
public:
    KeyViewItem(const QString &id, const QString &name) : QStandardItem(), keyId_(id) { setText(name); }

    QString keyId() const { return keyId_; }

private:
    QString keyId_;
};

class KeyViewProxyModel : public QSortFilterProxyModel {
public:
    KeyViewProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
    {
        for (int column = 0; column <= 1; ++column) {
            QModelIndex index = sourceModel()->index(sourceRow, column, sourceParent);
            if (index.data(Qt::DisplayRole).toString().contains(filterRegExp()))
                return true;
        }
        return false;
    }
};

PGPKeyDlg::PGPKeyDlg(Type t, const QString &defaultKeyID, QWidget *parent) : QDialog(parent), m_model(nullptr)
{
    m_ui.setupUi(this);
    setModal(true);

    m_pb_dtext = m_ui.buttonBox->addButton(tr("&Diagnostics"), QDialogButtonBox::ActionRole);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(QStringList() << tr("Key ID") << tr("User ID"));
    m_proxy = new KeyViewProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_ui.lv_keys->setModel(m_proxy);

    m_ui.lv_keys->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui.lv_keys, SIGNAL(doubleClicked(const QModelIndex &)), SLOT(doubleClicked(const QModelIndex &)));
    connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(do_accept()));
    connect(m_ui.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(reject()));
    connect(m_pb_dtext, SIGNAL(clicked()), SLOT(show_info()));
    connect(m_ui.le_filter, SIGNAL(textChanged(const QString &)), this, SLOT(filterTextChanged()));

    m_ui.le_filter->installEventFilter(this);

    KeyViewItem *firstItem    = nullptr;
    KeyViewItem *selectedItem = nullptr;
    int          rowIdx       = 0;

    const QStringList &&showSecretKeys
        = { "--with-fingerprint", "--list-secret-keys", "--with-colons", "--fixed-list-mode" };
    const QStringList &&showPublicKeys
        = { "--with-fingerprint", "--list-public-keys", "--with-colons", "--fixed-list-mode" };

    QString    keysRaw;
    GpgProcess gpg;

    if (t == Secret || t == All) {
        gpg.start(showSecretKeys);
        gpg.waitForFinished();
        keysRaw.append(QString::fromUtf8(gpg.readAll()));
    }
    if (t == Public || t == All) {
        gpg.start(showPublicKeys);
        gpg.waitForFinished();
        keysRaw.append(QString::fromUtf8(gpg.readAll()));
    }

    QString uid;
    const QStringList &&keysList = keysRaw.split("\n");
    for (const QString &line : keysList) {
        const QString &&type = line.section(':', 0, 0);
        QStandardItem * root = m_model->invisibleRootItem();

        if (type == "pub" || type == "sec") {
            uid                     = line.section(':', 9, 9);           // Used ID
            const QString &&longID  = line.section(':', 4, 4).right(16); // Long ID
            const QString &&shortID = line.section(':', 4, 4).right(8);  // Short ID

            KeyViewItem *i  = new KeyViewItem(longID, shortID);
            KeyViewItem *i2 = new KeyViewItem(longID, QString());
            root->setChild(rowIdx, 0, i);
            root->setChild(rowIdx, 1, i2);
            ++rowIdx;

            if (!defaultKeyID.isEmpty() && shortID == defaultKeyID) {
                selectedItem = i;
            }

            if (!firstItem) {
                firstItem = i;
            }
        } else if (type == "uid") {
            if (rowIdx >= 1) {
                QStandardItem *i2 = root->child(rowIdx - 1, 1);

                if (i2->text().isEmpty()) {
                    const QString &&name = line.section(':', 9, 9); // Name
                    i2->setText(name);
                }
            }
        }
    }

    if (selectedItem) {
        firstItem = selectedItem;
    }

    if (firstItem) {
        QModelIndex realIndex = m_model->indexFromItem(firstItem);
        QModelIndex fakeIndex = m_proxy->mapFromSource(realIndex);
        m_ui.lv_keys->setCurrentIndex(fakeIndex);
        m_ui.lv_keys->scrollTo(fakeIndex);
    }

    // adjustSize();
}

const QString &PGPKeyDlg::keyId() const { return m_keyId; }

bool PGPKeyDlg::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_ui.le_filter && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down || ke->key() == Qt::Key_PageUp
            || ke->key() == Qt::Key_PageDown || ke->key() == Qt::Key_Home || ke->key() == Qt::Key_End) {
            QCoreApplication::instance()->sendEvent(m_ui.lv_keys, event);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void PGPKeyDlg::filterTextChanged() { m_proxy->setFilterWildcard(m_ui.le_filter->text()); }

void PGPKeyDlg::doubleClicked(const QModelIndex &index)
{
    m_ui.lv_keys->setCurrentIndex(index);
    do_accept();
}

void PGPKeyDlg::do_accept()
{
    QModelIndex fakeIndex = m_ui.lv_keys->currentIndex();
    QModelIndex realIndex = m_proxy->mapToSource(fakeIndex);

    QStandardItem *item = m_model->itemFromIndex(realIndex);
    KeyViewItem *  i    = dynamic_cast<KeyViewItem *>(item);
    if (!i) {
        QMessageBox::information(this, tr("Error"), tr("Please select a key."));
        return;
    }
    m_keyId = i->keyId();
    accept();
}

void PGPKeyDlg::show_info()
{
    GpgProcess gpg;
    QString    info;

    gpg.info(info);
    ShowTextDlg *w = new ShowTextDlg(info, true, false, this);
    w->setWindowTitle(CAP(tr("GnuPG info")));
    w->resize(560, 240);
    w->show();
}
