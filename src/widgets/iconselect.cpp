/*
 * iconselect.cpp - class that allows user to select an PsiIcon from an Iconset
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "iconselect.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QLayout>
#include <QAbstractButton>
#include <QLabel>
#include <QTextCodec>
#include <QEvent>
#include <QMouseEvent>
#include <QWidgetAction>

#include <math.h>

#include "iconset.h"
#include "psitooltip.h"

//----------------------------------------------------------------------------
// IconSelectButton
//----------------------------------------------------------------------------

//! \if _hide_doc_
/**
	\class IconSelectButton
	\brief This button is used by IconSelect and displays one PsiIcon
*/
class IconSelectButton : public QAbstractButton
{
	Q_OBJECT

private:
	PsiIcon *ic;
	QSize s;
	bool animated;

public:
	IconSelectButton(QWidget *parent)
	: QAbstractButton(parent)
	{
		ic = 0;
		animated = false;
		connect (this, SIGNAL(clicked()), SLOT(iconSelected()));
	}

	~IconSelectButton()
	{
		iconStop();

		if ( ic ) {
			delete ic;
			ic = 0;
		}
	}

	void setIcon(const PsiIcon *i)
	{
		iconStop();

		if ( ic ) {
			delete ic;
			ic = 0;
		}

		if ( i )
			ic = new PsiIcon(*((PsiIcon *)i));
		else
			ic = 0;
	}

	const PsiIcon *icon() const
	{
		return ic;
	}

	QSize sizeHint() const { return s; }
	void setSizeHint(QSize sh) { s = sh; }

signals:
	void iconSelected(const PsiIcon *);
	void textSelected(QString);

private:
	void iconStart()
	{
		if ( ic ) {
			connect(ic, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
			if ( !animated ) {
				ic->activated(false);
				animated = true;
			}

			if ( !ic->text().isEmpty() ) {
				// and list of possible variants in the ToolTip
				QStringList toolTip;
				foreach(PsiIcon::IconText t, ic->text()) {
					toolTip += t.text;
				}

				QString toolTipText = toolTip.join(", ");
				if ( toolTipText.length() > 30 )
					toolTipText = toolTipText.left(30) + "...";

				setToolTip(toolTipText);
			}
		}
	}

	void iconStop()
	{
		if ( ic ) {
			disconnect(ic, 0, this, 0 );
			if ( animated ) {
				ic->stop();
				animated = false;
			}
		}
	}

	void enterEvent(QEvent *) { iconStart(); setFocus();  update(); } // focus follows mouse mode
	void leaveEvent(QEvent *) { iconStop(); clearFocus(); update(); }

private slots:
	void iconUpdated()
	{
		update();
	}

	void iconSelected()
	{
		clearFocus();
		if ( ic ) {
			emit iconSelected(ic);
			emit textSelected(ic->defaultText());
		}
	}

private:
	// reimplemented
	void paintEvent(QPaintEvent *)
	{
		QPainter p(this);
		QFlags<QStyle::StateFlag> flags = QStyle::State_Active | QStyle::State_Enabled;

		if ( hasFocus() )
			flags |= QStyle::State_Selected;

		QStyleOptionMenuItem opt;
		opt.palette = palette();
		opt.state = flags;
		opt.font = font();
		opt.rect = rect();
		style()->drawControl(QStyle::CE_MenuItem, &opt, &p, this);

		if ( ic ) {
			QPixmap pix = ic->pixmap();
			p.drawPixmap((width() - pix.width())/2, (height() - pix.height())/2, pix);
		}
	}
};
//! \endif

//----------------------------------------------------------------------------
// IconSelect -- the widget that does all dirty work
//----------------------------------------------------------------------------

class IconSelect : public QWidget
{
	Q_OBJECT

private:
	IconSelectPopup *menu;
	Iconset is;
	QGridLayout *grid;
	bool shown;

signals:
	void updatedGeometry();

public:
	IconSelect(IconSelectPopup *parentMenu);
	~IconSelect();

	void setIconset(const Iconset &);
	const Iconset &iconset() const;

protected:
	void noIcons();
	void createLayout();

	void paintEvent(QPaintEvent *)
	{
		QPainter p(this);

		QStyleOptionMenuItem opt;
		opt.palette = palette();
		opt.rect = rect();
		style()->drawControl(QStyle::CE_MenuEmptyArea, &opt, &p, this);
	}

protected slots:
	void closeMenu();
};

IconSelect::IconSelect(IconSelectPopup *parentMenu)
: QWidget(parentMenu)
{
	menu = parentMenu;
	connect(menu, SIGNAL(textSelected(QString)), SLOT(closeMenu()));

	grid = 0;
	noIcons();
}

IconSelect::~IconSelect()
{
}

void IconSelect::closeMenu()
{
	// this way all parent menus (if any) would be closed too
	QMouseEvent me(QEvent::MouseButtonPress, menu->pos() - QPoint(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	menu->mousePressEvent(&me);

	// just in case
	menu->close();
}

void IconSelect::createLayout()
{
	Q_ASSERT(!grid);
	grid = new QGridLayout(this);
	grid->setMargin(style()->pixelMetric(QStyle::PM_MenuPanelWidth, 0, this));
	grid->setSpacing(1);
}

void IconSelect::noIcons()
{
	createLayout();
	QLabel *lbl = new QLabel(this);
	grid->addWidget(lbl, 0, 0);
	lbl->setText( tr("No icons available") );
	emit updatedGeometry();
}

void IconSelect::setIconset(const Iconset &iconset)
{
	is = iconset;

	// delete all children
	if (grid) {
		delete grid;
		grid = 0;

		QObjectList list = children();
		qDeleteAll(list);
	}

	if ( !is.count() ) {
		noIcons();
		return;
	}

	// first we need to find optimal size for elements and don't forget about
	// taking too much screen space
	float w = 0, h = 0;

	double count; // the 'double' type is somewhat important for MSVC.NET here
	QListIterator<PsiIcon *> it = is.iterator();
	for (count = 0; it.hasNext(); count++) {
		PsiIcon *icon = it.next();
		w += icon->pixmap().width();
		h += icon->pixmap().height();
	}
	w /= count;
	h /= count;

	const int margin = 2;
	int tileSize = (int)qMax(w, h) + 2*margin;

	QRect r = QApplication::desktop()->availableGeometry( menu );
	int maxSize = qMin(r.width(), r.height())*3/4;

	int size = (int)ceil( sqrt( count ) );

	if ( size*tileSize > maxSize ) { // too many icons. find reasonable size.
		int c = 0;
		for (w = 0; w <= maxSize; w += tileSize)
			c++;
		size = c - 1;
	}

	// now, fill grid with elements
	createLayout();
	count = 0;

	int row = 0;
	int column = 0;
	it = is.iterator();
	while ( it.hasNext() ) {
		if ( ++count > size*size )
			break;

		IconSelectButton *b = new IconSelectButton(this);
		grid->addWidget(b, row, column);
		b->setIcon( it.next() );
		b->setSizeHint( QSize(tileSize, tileSize) );
		connect (b, SIGNAL(iconSelected(const PsiIcon *)), menu, SIGNAL(iconSelected(const PsiIcon *)));
		connect (b, SIGNAL(textSelected(QString)), menu, SIGNAL(textSelected(QString)));

		//connect (menu, SIGNAL(aboutToShow()), b, SLOT(aboutToShow()));
		//connect (menu, SIGNAL(aboutToHide()), b, SLOT(aboutToHide()));

		if (++column >= size) {
			++row;
			column = 0;
		}
	}
	emit updatedGeometry();
}

const Iconset &IconSelect::iconset() const
{
	return is;
}

//----------------------------------------------------------------------------
// IconSelectPopup
//----------------------------------------------------------------------------

class IconSelectPopup::Private : public QObject
{
	Q_OBJECT
public:
	Private(IconSelectPopup *parent)
		: QObject(parent)
		, parent_(parent)
	{}

	IconSelectPopup* parent_;
	IconSelect *icsel_;
	QWidgetAction* widgetAction_;

public slots:
	void updatedGeometry()
	{
		widgetAction_->setDefaultWidget(icsel_);
		parent_->removeAction(widgetAction_);
		parent_->addAction(widgetAction_);
	}
};

IconSelectPopup::IconSelectPopup(QWidget *parent)
: QMenu(parent)
{
	d = new Private(this);
	d->icsel_ = new IconSelect(this);
	d->widgetAction_ = new QWidgetAction(this);
	connect(d->icsel_, SIGNAL(updatedGeometry()), d, SLOT(updatedGeometry()));
	d->updatedGeometry();
}

IconSelectPopup::~IconSelectPopup()
{
}

void IconSelectPopup::setIconset(const Iconset &i)
{
	d->icsel_->setIconset(i);
}

const Iconset &IconSelectPopup::iconset() const
{
	return d->icsel_->iconset();
}

/**
	It's used by child widget to close the menu by simulating a
	click slightly outside of menu. This seems to be the best way
	to achieve this.
*/
void IconSelectPopup::mousePressEvent(QMouseEvent *e)
{
	QMenu::mousePressEvent(e);
}

#include "iconselect.moc"
