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

#include "common.h"
#include "mainwin_p.h"

#include <qapplication.h>
#include <qstyle.h>
#include <q3toolbar.h>
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

//----------------------------------------------------------------------------
// PopupActionButton
//----------------------------------------------------------------------------

class PopupActionButton : public QPushButton
{
	Q_OBJECT
public:
	PopupActionButton(QWidget *parent = 0, const char *name = 0);
	~PopupActionButton();

	void setIcon(Icon *, bool showText);
	void setLabel(QString);
	QSize sizeHint() const;

private slots:
	void pixmapUpdated(const QPixmap &);

private:
	void update();
	void paintEvent(QPaintEvent *);
	bool hasToolTip;
	Icon *icon;
	bool showText;
	QString label;
};

PopupActionButton::PopupActionButton(QWidget *parent, const char *name)
: QPushButton(parent, name)
{
	hasToolTip = false;
	icon = 0;
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

void PopupActionButton::setIcon(Icon *i, bool st)
{
	if ( icon ) {
		icon->stop();
		disconnect (icon, 0, this, 0);
		icon = 0;
	}

	icon = i;
	showText = st;

	if ( icon ) {
		pixmapUpdated(icon->pixmap());

		connect(icon, SIGNAL(pixmapChanged(const QPixmap &)), SLOT(pixmapUpdated(const QPixmap &)));
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

void PopupActionButton::pixmapUpdated(const QPixmap &pix)
{
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
	Icon *icon;
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

void PopupAction::setAlert (const Icon *icon)
{
	setIcon(icon, d->showText, true);
}

void PopupAction::setIcon (const Icon *icon, bool showText, bool alert)
{
	Icon *oldIcon = 0;
	if ( d->icon ) {
		oldIcon = d->icon;
		d->icon = 0;
	}

	d->showText = showText;

	if ( icon ) {
		if ( !alert )
			d->icon = new Icon(*icon);
		else
			d->icon = new AlertIcon(icon);

		IconAction::setIconSet(*icon);
	}
	else {
		d->icon = 0;
		IconAction::setIconSet(QIcon());
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

bool PopupAction::addTo (QWidget *w)
{
	if ( w->inherits("QToolBar") || w->inherits("Q3ToolBar") ) {
		QByteArray bname((const char*) (QString(name()) + QString("_action_button")));
		PopupActionButton *btn = new PopupActionButton ( (Q3ToolBar*)w, bname );
		d->buttons.append ( btn );
		btn->setMenu ( menu() );
		btn->setLabel ( text() );
		btn->setIcon ( d->icon, d->showText );
		btn->setSizePolicy ( d->size );
		btn->setEnabled ( isEnabled() );

		connect( btn, SIGNAL( destroyed() ), SLOT( objectDestroyed() ) );
	}
	else
		return IconAction::addTo(w);

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
// MTray
//----------------------------------------------------------------------------

class MTray::Private
{
public:
	Private() {
		icon	= 0;
		ti	= 0;
	}

	~Private() {
		if ( icon )
			delete icon;

		delete ti;
	}

	TrayIcon *ti;
	Icon *icon;

	QPixmap makeIcon();
	QRgb pixelBlend(QRgb p1, QRgb p2);
};

MTray::MTray(const QString &tip, QMenu *popup, QObject *parent)
:QObject(parent)
{
	d = new Private;

	d->ti = new TrayIcon(d->makeIcon(), tip, popup);
	d->ti->setWMDock(option.isWMDock);
	connect(d->ti, SIGNAL(clicked(const QPoint &, int)), SIGNAL(clicked(const QPoint &, int)));
	connect(d->ti, SIGNAL(doubleClicked(const QPoint &)), SIGNAL(doubleClicked(const QPoint &)));
	connect(d->ti, SIGNAL(closed()), SIGNAL(closed()));
	connect(qApp, SIGNAL(newTrayOwner()), d->ti, SLOT(newTrayOwner()));
	connect(qApp, SIGNAL(trayOwnerDied()), d->ti, SLOT(hide()));
	d->ti->show();
}

MTray::~MTray()
{
	delete d;
}

void MTray::setToolTip(const QString &str)
{
	d->ti->setToolTip(str);
}

void MTray::setIcon(const Icon *icon, bool alert)
{
	if ( d->icon ) {
		disconnect(d->icon, 0, this, 0 );
		d->icon->stop();

		delete d->icon;
		d->icon = 0;
	}

	if ( icon ) {
		if ( !alert )
			d->icon = new Icon(*icon);
		else
			d->icon = new AlertIcon(icon);

		connect(d->icon, SIGNAL(pixmapChanged(const QPixmap &)), SLOT(animate()));
		d->icon->activated();
	}
	else
		d->icon = new Icon();

	animate();
}

void MTray::setAlert(const Icon *icon)
{
	setIcon(icon, true);
}

bool MTray::isAnimating() const
{
	return d->icon->isAnimated();
}

bool MTray::isWMDock()
{
	return d->ti->isWMDock();
}

void MTray::show()
{
	d->ti->show();
}

void MTray::hide()
{
	d->ti->hide();
}


// a function to blend 2 pixels taking their alpha channels
// into consideration
// p1 is in the 1st layer, p2 is in the 2nd layer (over p1)
QRgb MTray::Private::pixelBlend(QRgb p1, QRgb p2)
{
	int a2 = qAlpha(p2);
	if (a2 == 255) return p2; // don't calculate anything if p2 is completely opaque
	int a1 = qAlpha(p1);
	double prop1 = double(a1*(255-a2))/double(255*255);
	double prop2 = double(a2)/255.0;
	int r = int( qRed(p1)*prop1 + qRed(p2)*prop2 );
	int g = int( qGreen(p1)*prop1 + qGreen(p2)*prop2 );
	int b = int( qBlue(p1)*prop1 + qBlue(p2)*prop2 );
	return qRgba(r, g, b, (a1>a2) ? a1:a2);
}


QPixmap MTray::Private::makeIcon()
{
	if ( !icon )
		return QPixmap();

#ifdef Q_WS_X11
	// on X11, the KDE dock is 22x22.  let's make our icon "seem" bigger.
	QImage real(22,22,32);
	QImage in = icon->image();
	in.detach();
	real.setAlphaBuffer(true);

	// make sure it is no bigger than 16x16
	if(in.width() > 16 || in.height() > 16)
		in = in.smoothScale(16,16);

	int xo = (real.width() - in.width()) / 2;
	int yo = (real.height() - in.height()) / 2;

	int n, n2;

	// clear the output and make it transparent
	// deprecates real.fill(0)
	for(n2 = 0; n2 < real.height(); ++n2)
		for(n = 0; n < real.width(); ++n)
			real.setPixel(n, n2, qRgba(0,0,0,0));

	// draw a dropshadow
	for(n2 = 0; n2 < in.height(); ++n2) {
		for(n = 0; n < in.width(); ++n) {
			if(int a = qAlpha(in.pixel(n,n2))) {
				int x = n + xo + 2;
				int y = n2 + yo + 2;
				real.setPixel(x, y, qRgba(0x80,0x80,0x80,a));
			}
		}
	}

	// draw the image
	for(n2 = 0; n2 < in.height(); ++n2) {
		for(n = 0; n < in.width(); ++n) {
			if(qAlpha(in.pixel(n,n2))) {
				QRgb pold = real.pixel(n + xo , n2 + yo);
				QRgb pnew = in.pixel(n , n2);
				real.setPixel(n + xo, n2 + yo, pixelBlend(pold, pnew));
			}
		}
	}

	QPixmap pixmap;
	pixmap.convertFromImage(real);
	return pixmap;
#else
	return icon->pixmap();
#endif
}

void MTray::animate()
{
#ifdef Q_WS_X11
	if ( !d->icon )
		return;

	QString cachedName = "PsiTray/" + option.defaultRosterIconset + "/" + d->icon->name() + "/" + QString::number( d->icon->frameNumber() );

	QPixmap p;
	if ( !QPixmapCache::find(cachedName, p) ) {
		p = d->makeIcon();
		QPixmapCache::insert( cachedName, p );
	}

	d->ti->setIcon(p);
#else
	d->ti->setIcon( d->makeIcon() );
#endif
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
		for ( PsiAccountListIt it(psi->accountList(TRUE)); it.current(); ++it, i++ )
		{
			PsiAccount *acc = it.current();
			pm->insertItem( acc->name(), parent(), SLOT(itemActivated(int)), 0, id*1000 + i );
			pm->setItemParameter ( id*1000 + i, i );
		}
		return pm;
	}

	void updateToolButton(QToolButton *btn)
	{
		if (psi->accountList(TRUE).count() >= 2) {
			btn->setMenu(subMenu(btn));
			disconnect(btn, SIGNAL(clicked()), sm, SLOT(map()));
		}
		else {
			btn->setMenu(0);
			connect(btn, SIGNAL(clicked()), sm, SLOT(map()));
		}
	}
};

MAction::MAction(Icon i, const QString &s, int id, PsiCon *psi, QObject *parent)
: IconAction(s, s, 0, parent)
{
	init (i, id, psi);
}

MAction::MAction(const QString &s, int id, PsiCon *psi, QObject *parent)
: IconAction(s, s, 0, parent)
{
	init (Icon(), id, psi);
}

void MAction::init(Icon i, int id, PsiCon *psi)
{
	d = new Private(id, psi, this);
	setPsiIcon (&i);
	connect(psi, SIGNAL(accountCountChanged()), SLOT(numAccountsChanged()));
	setEnabled ( !d->psi->accountList(TRUE).isEmpty() );
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
		if ( d->psi->accountList(TRUE).count() < 2 ) {
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
	PsiAccountList list = (PsiAccountList)d->psi->accountList(TRUE);

	if (n >= (int)list.count()) // just in case
		return;

	emit activated((PsiAccount *)list.at(n), d->id);
}

void MAction::numAccountsChanged()
{
	setEnabled ( !d->psi->accountList(TRUE).isEmpty() );

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
	if ( w->inherits("QToolBar") || w->inherits("Q3ToolBar") ) {
		new StretchWidget(w);
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
	if ( w->inherits("QToolBar") || w->inherits("Q3ToolBar") ) {
		MLabel *label = new MLabel(w, "EventNotifierAction::MLabel");
		label->setText(text());
		d->labels.append(label);
		connect(label, SIGNAL(destroyed()), SLOT(objectDestroyed()));
		connect(label, SIGNAL(doubleClicked()), SIGNAL(activated()));
		connect(label, SIGNAL(clicked(int)), SIGNAL(clicked(int)));

		if ( d->hide )
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

	Q3PtrListIterator<MLabel> it ( d->labels );
	for ( ; it.current(); ++it) {
		MLabel *label = it.current();
		label->hide();
		Q3ToolBar *toolBar = (Q3ToolBar *)label->parent();

		QObjectList l = toolBar->queryList( "QWidget" );
		int found = 0;

		for ( QObjectList::ConstIterator it = l.begin(); it != l.end(); ++it) {
			if ( QString((*it)->name()).left(3) != "qt_" ) // misc internal Qt objects
				found++;
		}

		if ( found == 1 ) // only MLabel is on ToolBar
			toolBar->hide();
	}
}

void EventNotifierAction::show()
{
	d->hide = false;

	Q3PtrListIterator<MLabel> it ( d->labels );
	for ( ; it.current(); ++it) {
		MLabel *label = it.current();
		Q3ToolBar *toolBar = (Q3ToolBar *)label->parent();
		label->show();
		toolBar->show();
	}
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
