/*
 * iconaction.cpp - the QAction subclass that uses Icons and supports animation
 * Copyright (C) 2003-2005  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "iconaction.h"

#include <QChildEvent>
#include <QLayout>
#include <QMenu>
#include <QTimer>

#include "icontoolbutton.h"
#include "iconwidget.h"
#ifndef WIDGET_PLUGIN
#include "iconset.h"
#include "psioptions.h"
#else
class Iconset;
class PsiIcon;
#endif

//----------------------------------------------------------------------------
// IconAction
//----------------------------------------------------------------------------

class IconAction::Private : public QObject {
    Q_OBJECT
public:
    QList<IconToolButton *> buttons;
    PsiIcon *               icon;
#ifdef WIDGET_PLUGIN
    QString iconName;
#endif
    IconAction *action;

    Private(IconAction *act, QObject *parent)
    {
        icon   = nullptr;
        action = act;
        Q_ASSERT(action);

        if (parent && parent->isWidgetType())
            static_cast<QWidget *>(parent)->addAction(action);

#ifdef Q_OS_MAC
        action->setIconVisibleInMenu(false);
#endif
    }

    ~Private()
    {
#ifndef WIDGET_PLUGIN
        if (icon) {
            delete icon;
            icon = nullptr;
        }
#endif
    }

    void init(const QString &objectName, const QString &statusTip, QKeySequence shortcut, bool checkable)
    {
        action->setObjectName(objectName);
        action->setStatusTip(statusTip);
        action->setShortcut(shortcut);
        action->setCheckable(checkable);
    }
};

IconAction::IconAction(QObject *parent, const QString &name) : QAction(parent)
{
    d = new Private(this, parent);
    setObjectName(name);
}

IconAction::IconAction(const QString &statusTip, const QString &icon, const QString &text, QKeySequence accel,
                       QObject *parent, const QString &name, bool checkable) :
    QAction(text, parent)
{
    d = new Private(this, parent);
    d->init(name, statusTip, accel, checkable);

    setPsiIcon(icon);
}

IconAction::IconAction(const QString &statusTip, const QString &icon, const QString &text, QList<QKeySequence> accel,
                       QObject *parent, const QString &name, bool checkable) :
    QAction(text, parent)
{
    d = new Private(this, parent);
    d->init(name, statusTip, QKeySequence(), checkable);
    setShortcuts(accel);

    setPsiIcon(icon);
}

IconAction::IconAction(const QString &statusTip, const QString &text, QKeySequence accel, QObject *parent,
                       const QString &name, bool checkable) :
    QAction(text, parent)
{
    d = new Private(this, parent);
    d->init(name, statusTip, accel, checkable);
}

IconAction::IconAction(const QString &statusTip, const QString &text, QList<QKeySequence> accel, QObject *parent,
                       const QString &name, bool checkable) :
    QAction(text, parent)
{
    d = new Private(this, parent);
    d->init(name, statusTip, QKeySequence(), checkable);
    setShortcuts(accel);
}

IconAction::IconAction(const QString &text, QObject *parent, const QString &icon) : QAction(text, parent)
{
    d = new Private(this, parent);
    d->init(QString(), QString(), QKeySequence(), false);

    setPsiIcon(icon);
}

IconAction::~IconAction()
{
    // delete the buttons list before our own destruction
    qDeleteAll(d->buttons);
    d->buttons.clear();

    delete d;
}

const PsiIcon *IconAction::psiIcon() const { return d->icon; }

void IconAction::setPsiIcon(const PsiIcon *i)
{
#ifdef WIDGET_PLUGIN
    Q_UNUSED(i);
#else
    if (d->icon) {
        disconnect(d->icon, nullptr, this, nullptr);
        d->icon->stop();
        delete d->icon;
        d->icon = nullptr;
    }

    QIcon is;
    if (i) {
        d->icon = new PsiIcon(*i);
        connect(d->icon, SIGNAL(iconModified()), SLOT(iconUpdated()));
        // We newer use animated iconactions
        // d->icon->activated(true);

        is = d->icon->icon();
    }

    QAction::setIcon(is);

    foreach (IconToolButton *btn, d->buttons)
        btn->setPsiIcon(d->icon);
#endif
}

void IconAction::setPsiIcon(const QString &name)
{
#ifdef WIDGET_PLUGIN
    d->iconName = name;
#else
    if (name.isEmpty()) {
        setPsiIcon(nullptr);
        return;
    }
    setPsiIcon(IconsetFactory::iconPtr(name));
#endif
}

QString IconAction::psiIconName() const
{
#ifndef WIDGET_PLUGIN
    if (d->icon)
        return d->icon->name();
#else
    return d->iconName;
#endif
    return QString();
}

bool IconAction::addTo(QWidget *w)
{
    w->addAction(this);
    return true;

    /*QStringList supportedContainers = {"QWidget"};
    if (w->inherits("QToolBar") ||
        supportedContainers.contains(w->metaObject()->className()))
    {
        QString bname = objectName() + "_action_button";
        IconToolButton *btn = new IconToolButton(w);
        btn->setObjectName(bname);
        d->buttons.append(btn);

        btn->setDefaultAction(this);

        btn->setText(text());
        btn->setPsiIcon(d->icon, false);

        btn->setDefaultAction(this);

        // need to explicitly update popupMode,
        // because setDefaultAction resets it
        btn->setPopupMode(QToolButton::InstantPopup);

        btn->setToolTip(toolTipFromMenuText());

        btn->setAutoRaise(true);
        btn->setFocusPolicy(Qt::NoFocus);

        if (supportedContainers.contains(w->metaObject()->className()))
            if (w->layout())
                w->layout()->addWidget(btn);

        connect(btn, SIGNAL(toggled(bool)), this, SLOT(setChecked(bool)));
        connect(btn, SIGNAL(destroyed()), SLOT(objectDestroyed()));

        addingToolButton(btn);
    }
    else
        w->addAction(this);

    return true;*/
}

void IconAction::objectDestroyed()
{
    QObject *obj = sender();
    d->buttons.removeAll(static_cast<IconToolButton *>(obj));
}

void IconAction::setChecked(bool b)
{
    QAction::setChecked(b);
    foreach (IconToolButton *btn, d->buttons)
        btn->setChecked(b);
}

void IconAction::toolButtonToggled(bool b) { setChecked(b); }

void IconAction::setEnabled(bool e)
{
    QAction::setEnabled(e);
    foreach (IconToolButton *btn, d->buttons)
        btn->setEnabled(e);
}

void IconAction::setText(const QString &t)
{
    QAction::setText(t);
    foreach (IconToolButton *btn, d->buttons)
        btn->setText(t);
}

QList<IconToolButton *> IconAction::buttonList() { return d->buttons; }

void IconAction::iconUpdated()
{
#ifndef WIDGET_PLUGIN
    QAction::setIcon(d->icon ? d->icon->icon() : QIcon());
#endif
}

QString IconAction::toolTipFromMenuText() const
{
    QString tt, str = text();
    for (int i = 0; i < int(str.length()); i++)
        if (str[i] == '&' && str[i + 1] != '&')
            continue;
        else
            tt += str[i];

    return tt;
}

void IconAction::setMenu(QMenu *p) { doSetMenu(p); }

void IconAction::doSetMenu(QMenu *p)
{
    QAction::setMenu(p);

    foreach (IconToolButton *btn, d->buttons) {
        btn->setMenu(nullptr);

        if (menu())
            btn->setMenu(menu());
    }
}

void IconAction::setIcon(const QIcon &ic)
{
    QAction::setIcon(ic);

    foreach (IconToolButton *btn, d->buttons)
        btn->setIcon(ic);
}

void IconAction::setVisible(bool b)
{
    QAction::setVisible(b);

    foreach (IconToolButton *btn, d->buttons) {
        if (b)
            btn->show();
        else
            btn->hide();
    }
}

IconAction *IconAction::copy() const
{
    IconAction *act
        = new IconAction(text(), psiIconName(), statusTip(), shortcut(), nullptr, objectName(), isCheckable());

    *act = *this;

    return act;
}

IconAction &IconAction::operator=(const IconAction &from)
{
    setText(from.text());
    setPsiIcon(from.psiIconName());
    setStatusTip(from.statusTip());
    setShortcut(from.shortcut());
    setObjectName(from.objectName());
    setCheckable(from.isCheckable());
    setToolTip(toolTip());

    // TODO: add more

    return *this;
}

void IconAction::setParent(QObject *newParent)
{
    QWidget *oldParent = qobject_cast<QWidget *>(parent());
    if (oldParent) {
        oldParent->removeAction(this);
    }

    QAction::setParent(newParent);
    if (newParent && newParent->isWidgetType()) {
        static_cast<QWidget *>(newParent)->addAction(this);
    }
}

//----------------------------------------------------------------------------
// IconActionGroup
//----------------------------------------------------------------------------

class IconActionGroup::Private : public QObject {
    Q_OBJECT
public:
    explicit Private(IconActionGroup *_group) { group = _group; }

    IconActionGroup *group = nullptr;
    QMenu *          popup = nullptr;

    bool exclusive    = false;
    bool usesDropDown = false;

    bool dirty = false;

public slots:
    void updatePopup();
};

void IconActionGroup::Private::updatePopup()
{
    if (!dirty)
        return;

    if (!usesDropDown)
        qWarning("IconActionGroup does not support !usesDropDown yet");

    popup->clear();

    QList<QAction *> list = group->findChildren<QAction *>();
    foreach (QAction *action, list) {
        if (!group->psiIcon() && action->inherits("IconAction"))
            group->setIcon(static_cast<IconAction *>(action)->icon());

        popup->addAction(action);
    }

    group->setMenu(popup);
    dirty = false;
}

IconActionGroup::IconActionGroup(QObject *parent, const char *name, bool exclusive) : IconAction(parent, name)
{
    d        = new Private(this);
    d->popup = new QMenu();
    d->dirty = true;
    setUsesDropDown(true);
    d->updatePopup();

    d->exclusive = exclusive;
#ifndef WIDGET_PLUGIN
    const QString css = PsiOptions::instance()->getOption("options.ui.contactlist.css").toString();
    if (!css.isEmpty()) {
        d->popup->setStyleSheet(css);
    }
#endif
}

IconActionGroup::~IconActionGroup()
{
    delete d->popup;
    delete d;
}

void IconActionGroup::childEvent(QChildEvent *e)
{
    IconAction::childEvent(e);

    d->dirty = true;
    QTimer::singleShot(0, d, SLOT(updatePopup()));
}

void IconActionGroup::add(QAction *) { qWarning("IconActionGroup::add(): not implemented"); }

void IconActionGroup::addSeparator()
{
    QAction *separatorAction = new QAction(this);
    separatorAction->setObjectName("separator_action");
    separatorAction->setSeparator(true);
}

bool IconActionGroup::addTo(QWidget *w)
{
    if (w->inherits("Q3PopupMenu") || w->inherits("QMenu")) {
        QMenu *popup = static_cast<QMenu *>(w);

        QList<QAction *> list = findChildren<QAction *>();
        foreach (QAction *action, list)
            popup->addAction(action);

        return true;
    }

    w->addAction(this);
    return true;
}

IconAction *IconActionGroup::copy() const
{
    qWarning("IconActionGroup::copy() doesn't work!");
    return static_cast<IconAction *>(const_cast<IconActionGroup *>(this));
}

void IconActionGroup::setExclusive(bool e) { d->exclusive = e; }

bool IconActionGroup::isExclusive() const { return d->exclusive; }

void IconActionGroup::setUsesDropDown(bool u) { d->usesDropDown = u; }

bool IconActionGroup::usesDropDown() const { return d->usesDropDown; }

void IconActionGroup::addingToolButton(IconToolButton *btn) { btn->setPopupMode(QToolButton::MenuButtonPopup); }

QMenu *IconActionGroup::popup() { return d->popup; }

#include "iconaction.moc"
