/*
 * statusmenu.h - helper class that displays available statuses using QMenu
 * Copyright (C) 2008-2010  Michail Pishchagin
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

#ifndef STATUSMENU_H
#define STATUSMENU_H

#include <QMenu>
#include <QMouseEvent>

#include "iconaction.h"
#include "psicon.h"
#include "xmpp_status.h"

class StatusMenu : public QMenu
{
    Q_OBJECT
protected:
    PsiCon* psi;
    QList<IconAction*> statusActs, presetActs;

    bool eventFilter(QObject *obj, QEvent *event);

public:
    StatusMenu(QWidget* parent, PsiCon* _psi );
    void fill();
    void clear();

public slots:
    void presetsChanged();
    void statusChanged(const XMPP::Status& status);

signals:
    void statusSelected(XMPP::Status::Type, bool);
    void statusPresetSelected(const XMPP::Status &status, bool withPriority, bool isManualStatus);
    void statusPresetDialogForced(const QString& presetName);

private:
    //It is empty here, because in global menu we'll use global action, but for account menu we'll create new action
    virtual void addChoose() = 0;
    virtual void addReconnect() = 0;

    void addStatus(XMPP::Status::Type type);
    void addPresets(IconActionGroup* parent = nullptr);
    XMPP::Status::Type actionStatus(const QAction* action) const;

private slots:
    void presetActivated();
    void changePresetsActivated();
    void statusActivated();
};

#endif // STATUSMENU_H
