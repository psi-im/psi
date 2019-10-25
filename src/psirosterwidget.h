/*
 * psirosterwidget.h
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSIROSTERWIDGET_H
#define PSIROSTERWIDGET_H

#include <QPointer>
#include <QWidget>

class ContactListDragModel;
class PsiContactList;
class PsiContactListView;
class PsiFilteredContactListView;
class QLineEdit;
class QMimeData;
class QSortFilterProxyModel;
class QStackedWidget;

class PsiRosterWidget : public QWidget {
    Q_OBJECT
public:
    PsiRosterWidget(QWidget *parent);
    ~PsiRosterWidget();

    void setContactList(PsiContactList *contactList);

public slots:
    void filterEditTextChanged(const QString &);
    void quitFilteringMode();
    void updateFilterMode();
    void setFilterModeEnabled(bool enabled);
    void clearFilterEdit();

private slots:
    void optionChanged(const QString &option);
    void showAgentsChanged(bool);
    void showHiddenChanged(bool);
    void showSelfChanged(bool);
    void showOfflineChanged(bool);
    void setShowStatusMsg(bool);

protected:
    bool eventFilter(QObject *obj, QEvent *e);

private:
    QPointer<PsiContactList>    contactList_;
    QStackedWidget *            stackedWidget_;
    QWidget *                   contactListPage_;
    QWidget *                   filterPage_;
    PsiContactListView *        contactListPageView_;
    PsiFilteredContactListView *filterPageView_;
    QLineEdit *                 filterEdit_;

    ContactListDragModel * contactListModel_;
    QSortFilterProxyModel *filterModel_;
};

#endif // PSIROSTERWIDGET_H
