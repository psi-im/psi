/*
 * mainwin_p.cpp - classes used privately by the main window.
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#include "mainwin_p.h"

#include "alerticon.h"
#include "icontoolbutton.h"
#include "iconwidget.h"
#include "psiaccount.h"
#include "psicontactlist.h"
#include "psitoolbar.h"
#include "stretchwidget.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>
#include <QPixmap>
#include <QPixmapCache>
#include <QSignalMapper>
#include <QStyle>
#include <QTimer>
#include <QtAlgorithms>

//----------------------------------------------------------------------------
// PopupActionButton
//----------------------------------------------------------------------------

class PopupActionButton : public QPushButton {
    Q_OBJECT
public:
    PopupActionButton(QWidget *parent = nullptr, const char *name = nullptr);
    ~PopupActionButton();

    void setIcon(PsiIcon *, bool showText);
    void setLabel(const QString &);

    // reimplemented
    QSize sizeHint() const;
    QSize minimumSizeHint() const;

private slots:
    void pixmapUpdated();

private:
    void updateText();
    void paintEvent(QPaintEvent *);

private:
    PsiIcon *icon;
    bool     showText;
    QString  label;
};

PopupActionButton::PopupActionButton(QWidget *parent, const char *name) :
    QPushButton(parent), icon(nullptr), showText(true)
{
    setObjectName(name);
}

PopupActionButton::~PopupActionButton()
{
    if (icon)
        icon->stop();
}

QSize PopupActionButton::sizeHint() const
{
    // hack to prevent QToolBar's extender button from displaying
    if (sizePolicy().horizontalPolicy() == QSizePolicy::Expanding)
        return QSize(16, 16);

    return QPushButton::sizeHint();
}

QSize PopupActionButton::minimumSizeHint() const { return QSize(16, 16); }

void PopupActionButton::setIcon(PsiIcon *i, bool st)
{
    if (icon) {
        icon->stop();
        disconnect(icon, nullptr, this, nullptr);
        icon = nullptr;
    }

    icon = i;
    if (showText != st) {
        showText = st;
        updateText();
    }

    if (icon) {
        pixmapUpdated();

        connect(icon, SIGNAL(pixmapChanged()), SLOT(pixmapUpdated()));
        icon->activated();
    }
}

void PopupActionButton::setLabel(const QString &lbl)
{
    if (label != lbl) {
        label = lbl;
        updateText();
    }
}

void PopupActionButton::updateText()
{
    if ((showText && !label.isEmpty()) && styleSheet().isEmpty()) {
        setStyleSheet("text-align: center");
    } else if ((!showText || label.isEmpty()) && styleSheet() == "text-align: center") {
        setStyleSheet(QString());
    }

    if (showText) {
        QPushButton::setText(label);
        QPushButton::setToolTip(label);
    } else {
        QPushButton::setText(QString());
        QPushButton::setToolTip(QString());
    }
}

void PopupActionButton::pixmapUpdated()
{
    QPixmap pix;
    if (icon) {
        auto fs = fontInfo().pixelSize();
        pix     = icon->pixmap(QSize(int(fs * BiggerTextIconK + .5), int(fs * BiggerTextIconK + .5)));
        if (pix.height() > fs * HugeIconButtonK) {
            pix = pix.scaledToHeight(int(fs * BiggerTextIconK + .5), Qt::SmoothTransformation);
        }
    }
    QPushButton::setIcon(pix);
    QPushButton::setIconSize(pix.size());
}

void PopupActionButton::paintEvent(QPaintEvent *p)
{
    // crazy code ahead!  watch out for potholes and deer.

    // this gets us the width of the "text area" on the button.
    // adapted from qt/src/styles/qcommonstyle.cpp and qt/src/widgets/qpushbutton.cpp

    if (showText) {
        QStyleOptionButton style_option;
        style_option.init(this);
        QRect r = style()->subElementRect(QStyle::SE_PushButtonContents, &style_option, this);

        if (menu())
            r.setWidth(r.width() - style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &style_option, this));
        // if(!QPushButton::icon().isNull())
        //     r.setWidth(r.width() - (QPushButton::icon().pixmap(QIcon::Small, QIcon::Normal, QIcon::Off).width()));

        // font metrics
        QFontMetrics fm(font());

        // w1 = width of button text, w2 = width of text area
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        int w1 = fm.horizontalAdvance(label);
#else
        int w1 = fm.width(label);
#endif
        int w2 = r.width();

        // backup original text
        QString oldtext = label;

        // button text larger than what will fit?
        if (w1 > w2) {

            // make a string that fits
            bool    found = false;
            QString newtext;
            int     n;
            for (n = oldtext.length(); n > 0; --n) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
                if (fm.horizontalAdvance(oldtext, n) < w2) {
#else
                if (fm.width(oldtext, n) < w2) {
#endif
                    found = true;
                    break;
                }
            }
            if (found)
                newtext = oldtext.mid(0, n);
            else
                newtext = "";

            // set the new text that fits.  updates must be off, or we recurse.
            if (newtext != text()) {
                setUpdatesEnabled(false);
                QPushButton::setText(newtext);
                setUpdatesEnabled(true);
            }
        } else {
            if (oldtext != text()) {
                setUpdatesEnabled(false);
                QPushButton::setText(oldtext);
                setUpdatesEnabled(true);
            }
        }
    }

    QPushButton::paintEvent(p);
}

//----------------------------------------------------------------------------
// PopupAction -- the IconButton with popup or QPopupMenu
//----------------------------------------------------------------------------

class PopupAction::Private : public QObject {
public:
    QSizePolicy                size;
    QList<PopupActionButton *> buttons;
    PsiIcon *                  icon;
    bool                       showText;

    Private(QObject *parent) : QObject(parent)
    {
        icon     = nullptr;
        showText = true;
    }

    ~Private()
    {
        qDeleteAll(buttons);
        buttons.clear();
        if (icon)
            delete icon;
    }
};

PopupAction::PopupAction(const QString &label, QMenu *_menu, QObject *parent, const char *name) :
    IconAction(label, label, 0, parent, name)
{
    d = new Private(this);
    setMenu(_menu);
}

void PopupAction::setSizePolicy(const QSizePolicy &p) { d->size = p; }

void PopupAction::setAlert(const PsiIcon *icon) { setIcon(icon, d->showText, true); }

void PopupAction::setIcon(const PsiIcon *icon, bool showText, bool alert)
{
    PsiIcon *oldIcon = nullptr;
    if (d->icon) {
        oldIcon = d->icon;
        d->icon = nullptr;
    }

    d->showText = showText;

    if (icon) {
        if (!alert)
            d->icon = new PsiIcon(*icon);
        else
            d->icon = new AlertIcon(icon);

        IconAction::setIcon(icon->icon());
    } else {
        d->icon = nullptr;
        IconAction::setIcon(QIcon());
    }

    for (PopupActionButton *btn : d->buttons) {
        btn->setIcon(d->icon, showText);
    }

    if (oldIcon) {
        delete oldIcon;
    }
}

void PopupAction::setText(const QString &text)
{
    for (PopupActionButton *btn : d->buttons) {
        btn->setLabel(text);
    }
}

bool PopupAction::addTo(QWidget *w)
{
    QToolBar *toolbar = dynamic_cast<QToolBar *>(w);
    if (toolbar) {
        PopupActionButton *btn = new PopupActionButton(w);
        btn->setObjectName(objectName() + QString("_action_button"));
        d->buttons.append(btn);
        btn->setMenu(menu());
        btn->setLabel(text());
        btn->setIcon(d->icon, d->showText);
        btn->setSizePolicy(d->size);
        btn->setEnabled(isEnabled());
        toolbar->addWidget(btn);

        connect(btn, SIGNAL(destroyed()), SLOT(objectDestroyed()));
    } else {
        return IconAction::addTo(w);
    }

    return true;
}

void PopupAction::objectDestroyed() { d->buttons.removeAll(static_cast<PopupActionButton *>(sender())); }

void PopupAction::setEnabled(bool e)
{
    IconAction::setEnabled(e);
    for (PopupActionButton *btn : d->buttons) {
        btn->setEnabled(e);
    }
}

IconAction *PopupAction::copy() const
{
    PopupAction *act = new PopupAction(text(), menu(), nullptr, objectName().toLatin1());

    *act = *this;

    return act;
}

PopupAction &PopupAction::operator=(const PopupAction &from)
{
    *(static_cast<IconAction *>(this)) = from;

    d->size = from.d->size;
    setIcon(from.d->icon);
    d->showText = from.d->showText;

    return *this;
}

//----------------------------------------------------------------------------
// MLabel -- a clickable label
//----------------------------------------------------------------------------

MLabel::MLabel(QWidget *parent, const char *name) : QLabel(parent)
{
    setObjectName(name);
    setMinimumWidth(48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

void MLabel::mouseReleaseEvent(QMouseEvent *e)
{
    emit clicked(int(e->button()));
    e->ignore();
}

void MLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit doubleClicked();

    e->ignore();
}

//----------------------------------------------------------------------------
// MAction
//----------------------------------------------------------------------------

MAction::MAction(PsiIcon i, const QString &s, int id, PsiCon *psi, QObject *parent) : IconActionGroup(parent)
{
    init(s, i, id, psi);
}

MAction::MAction(const QString &s, int id, PsiCon *psi, QObject *parent) : IconActionGroup(parent)
{
    init(s, PsiIcon(), id, psi);
}

void MAction::init(const QString &name, PsiIcon i, int id, PsiCon *psi)
{
    id_         = id;
    controller_ = psi;

    setText(name);
    setStatusTip(name);
    setPsiIcon(&i);

    connect(controller_, SIGNAL(accountCountChanged()), SLOT(numAccountsChanged()));
    connect(this, SIGNAL(triggered()), SLOT(slotActivated()));
    numAccountsChanged();
}

bool MAction::addTo(QWidget *widget)
{
    widget->addAction(this);
    return true;
}

QList<PsiAccount *> MAction::accounts() const { return controller_->contactList()->enabledAccounts(); }

void MAction::slotActivated()
{
    if (accounts().count() > 0) {
        emit activated(accounts().first(), id_);
    }
}

void MAction::actionActivated()
{
    QAction *action = static_cast<QAction *>(sender());
    int      num    = action->property("id").toInt();
    if (num >= 0 && num < accounts().count()) {
        emit activated(accounts().at(num), id_);
    }
}

void MAction::numAccountsChanged()
{
    setEnabled(accounts().count() > 0);

    qDeleteAll(findChildren<QAction *>());

    for (PsiAccount *account : accounts()) {
        QAction *act = new QAction(account->name(), this);
        act->setProperty("id", accounts().indexOf(account));
        connect(act, SIGNAL(triggered()), SLOT(actionActivated()));
    }
}

IconAction *MAction::copy() const
{
    MAction *act = new MAction(text(), id_, controller_, nullptr);

    *act = *this;

    return act;
}

MAction &MAction::operator=(const MAction &from)
{
    *(static_cast<IconAction *>(this)) = from;

    return *this;
}

void MAction::doSetMenu(QMenu *menu)
{
    IconActionGroup::doSetMenu(findChildren<QAction *>().count() > 1 ? menu : nullptr);
}

//----------------------------------------------------------------------------
// SpacerAction
//----------------------------------------------------------------------------

SpacerAction::SpacerAction(QObject *parent, const char *name) : IconAction(parent)
{
    setObjectName(name);
    setText(tr("<Spacer>"));
    setToolTip(tr("Spacer provides spacing to separate actions"));
}

SpacerAction::~SpacerAction() { }

bool SpacerAction::addTo(QWidget *w)
{
    QToolBar *toolbar = dynamic_cast<QToolBar *>(w);
    if (toolbar) {
        StretchWidget *stretch = new StretchWidget(w);
        toolbar->addWidget(stretch);
        return true;
    }

    return false;
}

IconAction *SpacerAction::copy() const { return new SpacerAction(nullptr); }

//----------------------------------------------------------------------------
// SeparatorAction
//----------------------------------------------------------------------------

SeparatorAction::SeparatorAction(QObject *parent, const char *name) :
    IconAction(tr("<Separator>"), tr("<Separator>"), 0, parent, name)
{
    setSeparator(true);
    setToolTip(tr("Separator"));
}

SeparatorAction::~SeparatorAction() { }

// we don't want QToolButtons when adding this action
// on toolbar
bool SeparatorAction::addTo(QWidget *w)
{
    w->addAction(this);
    return true;
}

IconAction *SeparatorAction::copy() const { return new SeparatorAction(nullptr); }

//----------------------------------------------------------------------------
// EventNotifierAction
//----------------------------------------------------------------------------

class EventNotifierAction::Private {
public:
    Private() = default;

    QList<MLabel *> labels;
    bool            hide = false;
    QString         message;
};

EventNotifierAction::EventNotifierAction(QObject *parent, const char *name) : IconAction(parent, name)
{
    d = new Private;
    setText(tr("<Event notifier>"));
    d->hide = true;
}

EventNotifierAction::~EventNotifierAction() { delete d; }

bool EventNotifierAction::addTo(QWidget *w)
{
    if (w) {
        MLabel *label = new MLabel(w, "EventNotifierAction::MLabel");
        label->setText(d->message);
        d->labels.append(label);
        connect(label, SIGNAL(destroyed()), SLOT(objectDestroyed()));
        connect(label, SIGNAL(doubleClicked()), SIGNAL(triggered()));
        connect(label, SIGNAL(clicked(int)), SIGNAL(clicked(int)));

        QToolBar *toolbar = dynamic_cast<QToolBar *>(w);
        if (!toolbar) {
            QLayout *layout = w->layout();
            if (layout)
                layout->addWidget(label);
        } else {
            toolbar->addWidget(label);
        }

        if (d->hide)
            hide();

        return true;
    }

    return false;
}

void EventNotifierAction::setMessage(const QString &m)
{
    d->message = m;

    for (MLabel *label : d->labels) {
        label->setText(d->message);
    }
}

void EventNotifierAction::objectDestroyed()
{
    MLabel *label = static_cast<MLabel *>(sender());
    d->labels.removeAll(label);
}

void EventNotifierAction::hide()
{
    d->hide = true;

    for (MLabel *label : d->labels) {
        label->hide();
        PsiToolBar *toolBar = dynamic_cast<PsiToolBar *>(label->parent());
        if (toolBar) {
            int found = 0;
            for (QWidget *widget : toolBar->findChildren<QWidget *>()) {
                if (!widget->objectName().startsWith("qt_")
                    && !QString(widget->metaObject()->className()).startsWith("QToolBar")
                    && !QString(widget->metaObject()->className()).startsWith("QMenu")
                    && QString(widget->metaObject()->className()) != "Oxygen::TransitionWidget") // dirty hack
                {
                    found++;
                }
            }

            if (found == 1) // only MLabel is on ToolBar
                // We should not hide toolbar, if it should be visible (user set `enabled` in options)
                // toolBar->hide();
                toolBar->updateVisibility();
        }
    }
}

void EventNotifierAction::show()
{
    d->hide = false;

    for (MLabel *label : d->labels) {
        label->show();
        QToolBar *toolBar = dynamic_cast<QToolBar *>(label->parent());
        if (toolBar)
            toolBar->show();
    }
}

void EventNotifierAction::updateVisibility()
{
    if (d->hide)
        hide();
    else
        show();
}

IconAction *EventNotifierAction::copy() const
{
    EventNotifierAction *act = new EventNotifierAction(nullptr);

    *act = *this;

    return act;
}

EventNotifierAction &EventNotifierAction::operator=(const EventNotifierAction &from)
{
    *(static_cast<IconAction *>(this)) = from;

    d->hide = from.d->hide;

    return *this;
}

#include "mainwin_p.moc"
