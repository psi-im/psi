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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include <QMenuItem>
#include <QEvent>
#include <QMouseEvent>

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
	QString text;
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

public slots:
	void aboutToShow() { iconStart(); }
	void aboutToHide() { iconStop();  }

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
				// first, try to get the text by priorities
				QStringList lang;
				lang << QString(QTextCodec::locale()).left(2); // most prioritent, is the local language
				lang << "";                                    // and then the language without name goes (international?)
				lang << "en";                                  // then real English

				QString str;
				QStringList::Iterator it = lang.begin();
				for ( ; it != lang.end(); ++it) {
					QHash<QString, QString>::const_iterator it2 = ic->text().find( *it );
					if ( it2 != ic->text().end() ) {
						str = it2.value();
						break;
					}
				}

				// if all fails, just get the first text
				if ( str.isEmpty() )
				{
					QHashIterator<QString, QString> it ( ic->text() );
					while ( it.hasNext() ) {
						it.next();

						if ( !it.value().isEmpty() ) {
							str = it.value();
							break;
						}
					}
				}

				if ( !str.isEmpty() )
					text = str;

				// and list of possible variants in the ToolTip
				QString toolTip;
				foreach ( QString icText, ic->text() ) {
					if ( !toolTip.isEmpty() )
						toolTip += ", ";
					toolTip += icText;
					break; // comment this to get list of iconsets
				}
				if ( toolTip.length() > 30 )
					toolTip = toolTip.left(30) + "...";
				setToolTip(toolTip);
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

	void enterEvent(QEvent *) { setFocus();   update(); } // focus follows mouse mode
	void leaveEvent(QEvent *) { clearFocus(); update(); }

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
			emit textSelected(text);
		}
	}

private:
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

public:
	IconSelect(IconSelectPopup *parentMenu);
	~IconSelect();

	void setIconset(const Iconset &);
	const Iconset &iconset() const;

protected:
	void noIcons();

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

void IconSelect::noIcons()
{
	grid = new QGridLayout(this);

	QLabel *lbl = new QLabel(this);
	grid->addWidget(lbl);
	lbl->setText( tr("No icons available") );
}

void IconSelect::setIconset(const Iconset &iconset)
{
	is = iconset;

	// delete all children
	if (grid) {
		delete grid;

		QList<QWidget *> list = findChildren<QWidget *>();
		QObject *object;
		foreach (object, list)
			delete object;
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
	int tileSize = (int)QMAX(w, h) + 2*margin;

	QRect r = QApplication::desktop()->availableGeometry( menu );
	int maxSize = QMIN(r.width(), r.height())*3/4;

	int size = (int)ceil( sqrt( count ) );

	if ( size*tileSize > maxSize ) { // too many icons. find reasonable size.
		int c = 0;
		for (w = 0; w <= maxSize; w += tileSize)
			c++;
		size = c - 1;
	}

	// now, fill grid with elements
	grid = new QGridLayout(this, size, size);

	count = 0;

	it = is.iterator();
	while ( it.hasNext() ) {
		if ( ++count > size*size )
			break;

		IconSelectButton *b = new IconSelectButton(this);
		grid->addWidget(b);
		b->setIcon( it.next() );
		b->setSizeHint( QSize(tileSize, tileSize) );
		connect (b, SIGNAL(iconSelected(const PsiIcon *)), menu, SIGNAL(iconSelected(const PsiIcon *)));
		connect (b, SIGNAL(textSelected(QString)), menu, SIGNAL(textSelected(QString)));

		connect (menu, SIGNAL(aboutToShow()), b, SLOT(aboutToShow()));
		connect (menu, SIGNAL(aboutToHide()), b, SLOT(aboutToHide()));
	}
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
public:
	Private(IconSelectPopup *parent)
	: QObject(parent)
	{
		icsel = new IconSelect(parent);
	}
	
	IconSelect *icsel;
};

IconSelectPopup::IconSelectPopup(QWidget *parent)
: QMenu(parent)
{
	QGridLayout *grid = new QGridLayout(this);
	grid->setMargin(style()->pixelMetric(QStyle::PM_MenuPanelWidth, 0, this));
	grid->setAutoAdd(true);
	
	d = new Private(this);
}

IconSelectPopup::~IconSelectPopup()
{
}

void IconSelectPopup::setIconset(const Iconset &i)
{
	d->icsel->setIconset(i);
}

const Iconset &IconSelectPopup::iconset() const
{
	return d->icsel->iconset();
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

/**
	Override QMenu's nasty sizeHint() and use standard QWidget one
	which honors layouts and child widgets.
*/
QSize IconSelectPopup::sizeHint() const
{
	return QWidget::sizeHint();
}

#include "iconselect.moc"
