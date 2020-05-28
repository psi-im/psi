/*
 * actionlist.cpp - the customizeable action list
 * Copyright (C) 2004  Michail Pishchagin
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

#include "actionlist.h"

#include "iconaction.h"

#include <QHash>
#include <QList>
#include <QPointer>

//----------------------------------------------------------------------------
// ActionList
//----------------------------------------------------------------------------

class ActionList::Private : public QObject {
    Q_OBJECT
public:
    Private() = default;
    Private(const Private &);

    QString                      name;
    int                          id = 0;
    QStringList                  sortedActions;
    QHash<QString, IconAction *> actions;
    bool                         autoDeleteActions = false;

public slots:
    void actionDestroyed(QObject *);
};

ActionList::ActionList(const QString &name, int id, bool autoDelete) : QObject()
{
    d                    = new Private();
    d->autoDeleteActions = autoDelete;

    d->name = name;
    d->id   = id;
}

ActionList::ActionList(const ActionList &from) : QObject() { d = new Private(*from.d); }

ActionList::~ActionList()
{
    if (d->autoDeleteActions)
        qDeleteAll(d->actions);
    delete d;
}

QString ActionList::name() const { return d->name; }

int ActionList::id() const { return d->id; }

IconAction *ActionList::action(const QString &name) const { return d->actions.value(name); }
IconAction *ActionList::copyAction(const QString &name, QObject *parent) const
{
    auto origAction = d->actions.value(name);
    if (origAction) {
        IconAction *action = origAction->copy();
        if (action) {
            action->setParent(parent);
            connect(origAction, &IconAction::destroyed, action, &QObject::deleteLater);
            return action;
        }
    }
    return nullptr;
}

const QStringList &ActionList::actions() const { return d->sortedActions; }

void ActionList::addAction(const QString &name, IconAction *action)
{
    d->sortedActions << name;

    if (action) {
        action->setObjectName(name);
        d->actions.insert(name, action);
        d->connect(action, SIGNAL(destroyed(QObject *)), d, SLOT(actionDestroyed(QObject *)));
        emit actionAdded(action);
    }
}

IconAction *ActionList::addActionToWidget(const QString &name, QWidget *w, QObject *actionParent)
{
    auto action = copyAction(name, actionParent);
    if (action) {
        if (action->addTo(w)) {
            return action;
        }
        delete action;
    }
    return nullptr;
}

void ActionList::clear()
{
    if (d->autoDeleteActions)
        qDeleteAll(d->actions);
    d->actions.clear();
}

ActionList::Private::Private(const Private &from) : QObject()
{
    name = from.name;
    id   = from.id;

    actions           = from.actions;
    autoDeleteActions = from.autoDeleteActions;

    sortedActions = from.sortedActions;
}

void ActionList::Private::actionDestroyed(QObject *obj)
{
    sortedActions.removeOne(obj->objectName());
    actions.remove(obj->objectName());
}

//----------------------------------------------------------------------------
// MetaActionList
//----------------------------------------------------------------------------

class MetaActionList::Private {
public:
    Private() { }

    QList<ActionList *> lists;
};

MetaActionList::MetaActionList() : QObject() { d = new Private(); }

MetaActionList::~MetaActionList()
{
    while (!d->lists.isEmpty())
        delete d->lists.takeFirst();
    delete d;
}

ActionList *MetaActionList::actionList(const QString &name) const
{
    for (ActionList *a : d->lists) {
        if (a->name() == name)
            return a;
    }

    return nullptr;
}

ActionList *MetaActionList::actionList(const QString &name, int id) const
{
    auto it
        = std::find_if(d->lists.begin(), d->lists.end(), [&](auto a) { return a->id() == id && a->name() == name; });
    if (it == d->lists.end())
        return nullptr;
    return *it;
}

QList<ActionList *> MetaActionList::actionLists(const unsigned int id) const
{
    QList<ActionList *> list;

    for (int i = 0; i < 32; i++) {
        if (!(id & (1u << i)))
            continue;

        for (ActionList *a : d->lists) {
            if (uint(a->id()) & (1u << i))
                list.append(a);
        }
    }

    return list;
}

ActionList MetaActionList::suitableActions(int id) const
{
    QList<ActionList *> lists = actionLists(uint(id));
    ActionList          actions("", 0, false);

    for (ActionList *list : lists) {
        QStringList           actionList = list->actions();
        QStringList::Iterator it2        = actionList.begin();
        for (; it2 != actionList.end(); ++it2)
            actions.addAction(*it2, list->action(*it2));
    }

    return actions;
}

QStringList MetaActionList::actionLists() const
{
    QStringList names;

    for (ActionList *l : d->lists)
        names << l->name();

    return names;
}

void MetaActionList::addList(ActionList *list)
{
    if (list)
        d->lists.append(list);
}

void MetaActionList::clear()
{
    for (ActionList *l : d->lists) {
        l->clear();
    }
}

#include "actionlist.moc"
