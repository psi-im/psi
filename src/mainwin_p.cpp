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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mainwin_p.h"

#include <qapplication.h>
#include <qstyle.h>
#include <QToolBar>
#include <qtimer.h>
#include <qsignalmapper.h>
#include <qobject.h>
#include <qpixmapcache.h>
#include <QPixmap>
#include <Q3Button>
#include <Q3PtrList>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include "psiaccount.h"
#include "stretchwidget.h"
#include "iconwidget.h"
#include "icontoolbutton.h"
#include "alerticon.h"
#include "psicontactlist.h"

//----------------------------------------------------------------------------
// PopupActionButton
//----------------------------------------------------------------------------

class PopupActionButton : public QPushButton
{
	Q_OBJECT
public:
	PopupActionButton(QWidget *parent = 0, const char *name = 0);
	~PopupActionButton();

	void setIcon(PsiIcon *, bool showText);
	void setLabel(QString);

	// reimplemented
	QSize sizeHint() const;
	QSize minimumSizeHint() const;

private slots:
	void pixmapUpdated();

private:
	void update();
	void paintEvent(QPaintEvent *);
	bool hasToolTip;
	PsiIcon *icon;
	bool showText;
	QString label;
};

PopupActionButton::PopupActionButton(QWidget *parent, const char *name)
: QPushButton(parent, name), hasToolTip(false), icon(0), showText(true)
{
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

QSize PopupActionButton::minimumSizeHint() const
{
	return QSize(16, 16);
}

void PopupActionButton::setIcon(PsiIcon *i, bool st)
{
	if ( icon ) {
		icon->stop();
		disconnect (icon, 0, this, 0);
		icon = 0;
	}

	icon = i;
	showText = st;

	if ( icon ) {
		pixmapUpdated();

		connect(icon, SIGNAL(pixmapChanged()), SLOT(pixmapUpdated()));
		icon->activated();
	}
}

void PopupActionButton::setLabel(QString lbl)
{
	label = lbl;
	update();
}

void PopupActionButton::update()
{
	if ( showText )
		QPushButton::setText(label);
	else
		QPushButton::setText("");
}

void PopupActionButton::pixmapUpdated()
{
	QPixmap pix = icon ? icon->pixmap() : QPixmap();
	QPushButton::setIcon(pix);
	QPushButton::setIconSize(pix.size());
	update();
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

		if(isMenuButton())
			r.setWidth(r.width() - style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &style_option, this));
		if(iconSet() && !iconSet()->isNull())
			r.setWidth(r.width() - (iconSet()->pixmap(QIcon::Small, QIcon::Normal, QIcon::Off).width()));

		// font metrics
		QFontMetrics fm(font());

		// w1 = width of button text, w2 = width of text area
		int w1 = fm.width(label);
		int w2 = r.width();

		// backup original text
		QString oldtext = label;

		// button text larger than what will fit?
		if(w1 > w2) {
			if( !hasToolTip ) {
				setToolTip(label);
				hasToolTip = TRUE;
			}

			// make a string that fits
			bool found = FALSE;
			QString newtext;
			int n;
			for(n = oldtext.length(); n > 0; --n) {
				if(fm.width(oldtext, n) < w2) {
					found = TRUE;
					break;
				}
			}
			if(found)
				newtext = oldtext.mid(0, n);
			else
				newtext = "";

			// set the new text that fits.  updates must be off, or we recurse.
			if (newtext != text()) {
				setUpdatesEnabled(false);
				QPushButton::setText(newtext);
				setUpdatesEnabled(true);
			}
		}
		else {
			if (oldtext != text()) {
				setUpdatesEnabled(false);
				QPushButton::setText(oldtext);
				setUpdatesEnabled(true);
			}

			if( hasToolTip ) {
				setToolTip("");
				hasToolTip = false;
			}
		}
	}

	QPushButton::paintEvent(p);
}

//----------------------------------------------------------------------------
// PopupAction -- the IconButton with popup or QPopupMenu
//----------------------------------------------------------------------------

class PopupAction::Private : public QObject
{
public:
	QSizePolicy size;
	Q3PtrList<PopupActionButton> buttons;
	PsiIcon *icon;
	bool showText;

	Private (QObject *parent)
	: QObject (parent)
	{
		icon = 0;
		showText = true;
	}

	~Private()
	{
		buttons.clear();
		if (icon)
			delete icon;
	}
};

PopupAction::PopupAction (const QString &label, QMenu *_menu, QObject *parent, const char *name)
: IconAction (label, label, 0, parent, name)
{
	d = new Private (this);
	setMenu( _menu );
}

void PopupAction::setSizePolicy (const QSizePolicy &p)
{
	d->size = p;
}

void PopupAction::setAlert (const PsiIcon *icon)
{
	setIcon(icon, d->showText, true);
}

void PopupAction::setIcon (const PsiIcon *icon, bool showText, bool alert)
{
	PsiIcon *oldIcon = 0;
	if ( d->icon ) {
		oldIcon = d->icon;
		d->icon = 0;
	}

	d->showText = showText;

	if ( icon ) {
		if ( !alert )
			d->icon = new PsiIcon(*icon);
		else
			d->icon = new AlertIcon(icon);

		IconAction::setIcon(icon->icon());
	}
	else {
		d->icon = 0;
		IconAction::setIcon(QIcon());
	}

	for ( Q3PtrListIterator<PopupActionButton> it(d->buttons); it.current(); ++it ) {
		PopupActionButton *btn = it.current();
		btn->setIcon (d->icon, showText);
	}

	if ( oldIcon ) {
		delete oldIcon;
	}
}

void PopupAction::setText (const QString &text)
{
	IconAction::setText (text);
	for ( Q3PtrListIterator<PopupActionButton> it(d->buttons); it.current(); ++it ) {
		PopupActionButton *btn = it.current();
		btn->setLabel (text);
	}
}

bool PopupAction::addTo(QWidget *w)
{
	QToolBar* toolbar = dynamic_cast<QToolBar*>(w);
	if (toolbar) {
		QByteArray bname((const char*)(QString(name()) + QString("_action_button")));
		PopupActionButton *btn = new PopupActionButton(w, bname);
		d->buttons.append(btn);
		btn->setMenu(menu());
		btn->setLabel(text());
		btn->setIcon(d->icon, d->showText);
		btn->setSizePolicy(d->size);
		btn->setEnabled(isEnabled());
		toolbar->addWidget(btn);

		connect(btn, SIGNAL(destroyed()), SLOT(objectDestroyed()));
	}
	else {
		return IconAction::addTo(w);
	}

	return true;
}

void PopupAction::objectDestroyed ()
{
	const QObject *obj = sender();
	d->buttons.removeRef( (PopupActionButton *) obj );
}

void PopupAction::setEnabled (bool e)
{
	IconAction::setEnabled (e);
	for ( Q3PtrListIterator<PopupActionButton> it(d->buttons); it.current(); ++it ) {
		PopupActionButton *btn = it.current();
		btn->setEnabled (e);
	}
}

IconAction *PopupAction::copy() const
{
	PopupAction *act = new PopupAction(text(), menu(), 0, name());

	*act = *this;

	return act;
}

PopupAction &PopupAction::operator=( const PopupAction &from )
{
	*( (IconAction *)this ) = from;

	d->size = from.d->size;
	setIcon( from.d->icon );
	d->showText = from.d->showText;

	return *this;
}

//----------------------------------------------------------------------------
// MLabel -- a clickable label
//----------------------------------------------------------------------------

MLabel::MLabel(QWidget *parent, const char *name)
:QLabel(parent, name)
{
	setMinimumWidth(48);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

void MLabel::mouseReleaseEvent(QMouseEvent *e)
{
	emit clicked(e->button());
	e->ignore();
}

void MLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
	if(e->button() == Qt::LeftButton)
		emit doubleClicked();

	e->ignore();
}


//----------------------------------------------------------------------------
// MAction
//----------------------------------------------------------------------------

class MAction::Private : public QObject
{
public:
	int id;
	PsiCon *psi;
	QSignalMapper *sm;

	Private (int _id, PsiCon *_psi, QObject *parent)
	: QObject (parent)
	{
		id = _id;
		psi = _psi;
		sm = new QSignalMapper(this, "MAction::Private::SignalMapper");
	}

	QMenu *subMenu(QWidget *p)
	{
		QMenu *pm = new QMenu (p);
		uint i = 0;
		foreach(PsiAccount* acc, psi->contactList()->enabledAccounts()) {
			pm->insertItem( acc->name(), parent(), SLOT(itemActivated(int)), 0, id*1000 + i );
			pm->setItemParameter ( id*1000 + i, i );
			i++;
		}
		return pm;
	}

	void updateToolButton(QToolButton *btn)
	{
		if (psi->contactList()->enabledAccounts().count() >= 2) {
			btn->setMenu(subMenu(btn));
			disconnect(btn, SIGNAL(clicked()), sm, SLOT(map()));
		}
		else {
			btn->setMenu(0);
			// connect exactly once:
			disconnect(btn, SIGNAL(clicked()), sm, SLOT(map()));
			connect(btn, SIGNAL(clicked()), sm, SLOT(map()));
		}
	}
};

MAction::MAction(PsiIcon i, const QString &s, int id, PsiCon *psi, QObject *parent)
: IconAction(s, s, 0, parent)
{
	init (i, id, psi);
}

MAction::MAction(const QString &s, int id, PsiCon *psi, QObject *parent)
: IconAction(s, s, 0, parent)
{
	init (PsiIcon(), id, psi);
}

void MAction::init(PsiIcon i, int id, PsiCon *psi)
{
	d = new Private(id, psi, this);
	setPsiIcon (&i);
	connect(psi, SIGNAL(accountCountChanged()), SLOT(numAccountsChanged()));
	setEnabled ( d->psi->contactList()->haveEnabledAccounts() );
	connect (d->sm, SIGNAL(mapped(int)), SLOT(itemActivated(int)));
}

bool MAction::addTo(QWidget *w)
{
	if ( w->inherits("QPopupMenu") || w->inherits("QMenu") )
	{
		QMenu *menu = (QMenu*)w;
        QIcon iconset;
#ifndef Q_WS_MAC
		iconset = iconSet();
#endif
		if ( d->psi->contactList()->enabledAccounts().count() < 2 ) {
			menu->insertItem ( iconset, menuText(), this, SLOT(itemActivated(int)), 0, d->id*1000 + 0 );
			menu->setItemEnabled (d->id*1000 + 0, isEnabled());
			menu->setItemParameter ( d->id*1000 + 0, 0 );
		}
		else
			menu->insertItem(iconset, menuText(), d->subMenu(w));
	}
	else
		return IconAction::addTo(w);

	return true;
}

void MAction::addingToolButton(IconToolButton *btn)
{
	d->sm->setMapping(btn, 0);

	d->updateToolButton(btn);
}

void MAction::itemActivated(int n)
{
	QList<PsiAccount*> list = d->psi->contactList()->enabledAccounts();

	if (n >= list.count()) // just in case
		return;

	emit activated((PsiAccount *)list.at(n), d->id);
}

void MAction::numAccountsChanged()
{
	setEnabled( d->psi->contactList()->haveEnabledAccounts() );

	QList<IconToolButton*> btns = buttonList();
	for ( QList<IconToolButton*>::Iterator it = btns.begin(); it != btns.end(); ++it ) {
		QToolButton *btn = (*it);

		if (btn->menu())
			delete btn->menu();

		d->updateToolButton(btn);
	}
}

IconAction *MAction::copy() const
{
	MAction *act = new MAction(text(), d->id, d->psi, 0);

	*act = *this;

	return act;
}

MAction &MAction::operator=( const MAction &from )
{
	*( (IconAction *)this ) = from;

	return *this;
}

//----------------------------------------------------------------------------
// SpacerAction
//----------------------------------------------------------------------------

SpacerAction::SpacerAction(QObject *parent, const char *name)
: IconAction(parent, name)
{
	setText(tr("<Spacer>"));
	setMenuText(tr("<Spacer>"));
	setWhatsThis(tr("Spacer provides spacing to separate actions"));
}

SpacerAction::~SpacerAction()
{
}

bool SpacerAction::addTo(QWidget *w)
{
	QToolBar* toolbar = dynamic_cast<QToolBar*>(w);
	if (toolbar) {
		StretchWidget* stretch = new StretchWidget(w);
		toolbar->addWidget(stretch);
		return true;
	}

	return false;
}

IconAction *SpacerAction::copy() const
{
	return new SpacerAction( 0 );
}

//----------------------------------------------------------------------------
// SeparatorAction
//----------------------------------------------------------------------------

SeparatorAction::SeparatorAction( QObject *parent, const char *name )
	: IconAction( tr("<Separator>"), tr("<Separator>"), 0, parent, name )
{
	setSeparator(true);
	setWhatsThis (tr("Separator"));
}

SeparatorAction::~SeparatorAction()
{
}

// we don't want QToolButtons when adding this action
// on toolbar
bool SeparatorAction::addTo(QWidget *w)
{
	w->addAction(this);
	return true;
}

IconAction *SeparatorAction::copy() const
{
	return new SeparatorAction(0);
}

//----------------------------------------------------------------------------
// EventNotifierAction
//----------------------------------------------------------------------------

class EventNotifierAction::Private
{
public:
	Private() { }

	Q3PtrList<MLabel> labels;
	bool hide;
};

EventNotifierAction::EventNotifierAction(QObject *parent, const char *name)
: IconAction(parent, name)
{
	d = new Private;
	setMenuText(tr("<Event notifier>"));
	d->hide = true;
}

EventNotifierAction::~EventNotifierAction()
{
	delete d;
}

bool EventNotifierAction::addTo(QWidget *w)
{
	if (w) {
		MLabel *label = new MLabel(w, "EventNotifierAction::MLabel");
		label->setText(text());
		d->labels.append(label);
		connect(label, SIGNAL(destroyed()), SLOT(objectDestroyed()));
		connect(label, SIGNAL(doubleClicked()), SIGNAL(activated()));
		connect(label, SIGNAL(clicked(int)), SIGNAL(clicked(int)));

		QToolBar* toolbar = dynamic_cast<QToolBar*>(w);
		if (!toolbar) {
			QLayout* layout = w->layout();
			if (layout)
				layout->addWidget(label);
		}
		else {
			toolbar->addWidget(label);
		}

		if (d->hide)
			hide();

		return true;
	}

	return false;
}

void EventNotifierAction::setText(const QString &t)
{
	IconAction::setText("<nobr>" + t + "</nobr>");

	Q3PtrListIterator<MLabel> it ( d->labels );
	for ( ; it.current(); ++it) {
		MLabel *label = it.current();
		label->setText(text());
	}
}

void EventNotifierAction::objectDestroyed()
{
	MLabel *label = (MLabel *)sender();
	d->labels.removeRef(label);
}

void EventNotifierAction::hide()
{
	d->hide = true;

	Q3PtrListIterator<MLabel> it(d->labels);
	for (; it.current(); ++it) {
		MLabel *label = it.current();
		label->hide();
		QToolBar *toolBar = dynamic_cast<QToolBar*>(label->parent());
		if (toolBar) {
			QObjectList l = toolBar->queryList("QWidget");
			int found = 0;

			for (QObjectList::ConstIterator it = l.begin(); it != l.end(); ++it) {
				if (QString((*it)->name()).left(3) != "qt_")   // misc internal Qt objects
					found++;
			}

			if (found == 1)   // only MLabel is on ToolBar
				toolBar->hide();
		}
	}
}

void EventNotifierAction::show()
{
	d->hide = false;

	Q3PtrListIterator<MLabel> it ( d->labels );
	for ( ; it.current(); ++it) {
		MLabel *label = it.current();
		label->show();
		QToolBar *toolBar = dynamic_cast<QToolBar*>(label->parent());
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
	EventNotifierAction *act = new EventNotifierAction( 0 );

	*act = *this;

	return act;
}

EventNotifierAction &EventNotifierAction::operator=( const EventNotifierAction &from )
{
	*( (IconAction *)this ) = from;

	d->hide = from.d->hide;

	return *this;
}

#include "mainwin_p.moc"
