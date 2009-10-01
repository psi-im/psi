/*
 * iconwidget.cpp - misc. Iconset- and PsiIcon-aware widgets
 * Copyright (C) 2003-2006  Michail Pishchagin
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

#include "iconwidget.h"
#include "iconsetdisplay.h"
#include "iconsetselect.h"
#include "icontoolbutton.h"
#include "iconbutton.h"

#include <QApplication>
#include <QPainter>
#include <QBrush>

#ifndef WIDGET_PLUGIN
#	include "iconset.h"
#	include <QStyle>
#	include <QBitmap>
#	include <QMap>
#       include "pixmaputil.h"
#else
#	include <QImage>

static const char *cancel_xpm[] = {
"22 22 60 1",
" 	c None",
".	c #E84300",
"+	c #E63F00",
"@	c #D11E00",
"#	c #D11B00",
"$	c #F69D50",
"%	c #F59A4D",
"&	c #E23800",
"*	c #EE5F1F",
"=	c #ED5A1D",
"-	c #CD1700",
";	c #FECBA2",
">	c #FEC69A",
",	c #F39045",
"'	c #DE3200",
")	c #FE7B3C",
"!	c #FE7234",
"~	c #EC4C15",
"{	c #CC1100",
"]	c #FEC091",
"^	c #FEBA89",
"/	c #F2873D",
"(	c #DA2C00",
"_	c #FE692C",
":	c #EB4712",
"<	c #CA0F00",
"[	c #FEB480",
"}	c #FEAE78",
"|	c #F07D35",
"1	c #D62600",
"2	c #FEA870",
"3	c #FEA166",
"4	c #EF722D",
"5	c #D32100",
"6	c #FE9B5F",
"7	c #FE9356",
"8	c #F16C2A",
"9	c #F16525",
"0	c #FE8B4D",
"a	c #FE8445",
"b	c #EE4B15",
"c	c #FE6025",
"d	c #EE4310",
"e	c #C90E00",
"f	c #FE561D",
"g	c #FE4B16",
"h	c #EA2F08",
"i	c #C70900",
"j	c #FE4010",
"k	c #FE350B",
"l	c #EA1D03",
"m	c #C60700",
"n	c #FE2906",
"o	c #FE1A02",
"p	c #E90900",
"q	c #C50300",
"r	c #FE0A00",
"s	c #FE0000",
"t	c #E90000",
"u	c #C40000",
"                      ",
"                      ",
"    .+          @#    ",
"   .$%&        @*=-   ",
"  .$;>,'      @*)!~{  ",
"  +%>]^/(    @*)!_:<  ",
"   &,^[}|1  @*)!_:<   ",
"    '/}2345@*)!_:<    ",
"     (|36789)!_:<     ",
"      1470a)!_:<      ",
"       58a)!_b<       ",
"       @9)!_cde       ",
"      @*)!_cfghi      ",
"     @*)!_bdgjklm     ",
"    @*)!_:<ehknopq    ",
"   @*)!_:<  ilorstu   ",
"  @*)!_:<    mpssstu  ",
"  #=!_:<      qtsstu  ",
"   -~:<        uttu   ",
"    {<          uu    ",
"                      ",
"                      "};
#endif

//----------------------------------------------------------------------------
// RealIconWidgetItem
//----------------------------------------------------------------------------

class RealIconWidgetItem : public IconWidgetItem
{
	Q_OBJECT
public:
	RealIconWidgetItem(QListWidget *parent = 0)
	: IconWidgetItem(parent) {}
	
	virtual void paint(QPainter *painter) const = 0;
	virtual int height() const = 0;
	virtual int width() const = 0;
	virtual QPoint textPosition(QPainter *painter) const = 0;

	static const int TextPositionRole = Qt::UserRole + 1;

	QVariant data(int role) const
	{
		if (role == Qt::SizeHintRole)
			return QVariant(QSize(width(), height()));
		else if (role == Qt::DecorationRole) {
#ifndef WIDGET_PLUGIN
			QPixmap pix = PixmapUtil::createTransparentPixmap(width(), height());
#else
			QPixmap pix(width(), height()); // junk inside
#endif
			QPainter p(&pix);
			paint(&p);
			p.end();
			return QVariant(pix);
		}
		else if (role == TextPositionRole) {
			QPixmap pix(width(), height());
			QPainter p(&pix);
			QPoint pos = textPosition(&p);
			p.end();
			return QVariant(pos);
		}
		return QListWidgetItem::data(role);
	}

public slots:
	void update()
	{
		setData(Qt::UserRole, data(Qt::UserRole).toInt() + 1);
	}
};

//----------------------------------------------------------------------------
// IconWidgetDelegate
//----------------------------------------------------------------------------

class IconWidgetDelegate : public QAbstractItemDelegate
{
public:
	IconWidgetDelegate(QObject *parent)
	: QAbstractItemDelegate(parent) {}

	QSize sizeHint(const QStyleOptionViewItem &/*option*/, const QModelIndex &index) const 
	{
		return index.model()->data(index, Qt::SizeHintRole).toSize();
	}
	
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		const QAbstractItemModel *model = index.model();

		QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
			? QPalette::Normal : QPalette::Disabled;

		// draw the background color
		if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
			painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
			painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
		} 
		else {
			painter->setPen(option.palette.color(cg, QPalette::Text));
			QVariant value = model->data(index, Qt::BackgroundRole);
			if (value.isValid() && qvariant_cast<QColor>(value).isValid())
				painter->fillRect(option.rect, qvariant_cast<QColor>(value));
		}
		
		painter->drawPixmap(option.rect.topLeft(), model->data(index, Qt::DecorationRole).value<QPixmap>());

		QVariant textPosition = model->data(index, RealIconWidgetItem::TextPositionRole);
		if (textPosition.isValid() && !qvariant_cast<QPoint>(textPosition).isNull())
			painter->drawText(option.rect.topLeft() + qvariant_cast<QPoint>(textPosition), model->data(index, Qt::DisplayRole).value<QString>());

		if (option.state & QStyle::State_HasFocus) {
			QStyleOptionFocusRect o;
			o.QStyleOption::operator=(option);
			QRect r = option.rect;
			QPoint margin(1, 1);
			o.rect = QRect(r.topLeft() + margin, r.bottomRight() - margin);
			o.state |= QStyle::State_KeyboardFocusChange;
			o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
				? QPalette::Highlight : QPalette::Background);
			QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
		}
	}
};

//----------------------------------------------------------------------------
// IconsetSelect
//----------------------------------------------------------------------------

class IconsetSelectItem : public RealIconWidgetItem
{
	Q_OBJECT
private:
	static const int margin;
	static const int displayNumIcons;
#ifndef WIDGET_PLUGIN
	Iconset iss;
	QMap<PsiIcon*, QRect> iconRects;
#endif
	int w, h;
	mutable int fullW, fullH;

public:
	IconsetSelectItem(QListWidget *parent, const Iconset &_iconset)
	: RealIconWidgetItem(parent)
	{
#ifndef WIDGET_PLUGIN
		iss = _iconset;
		setText( iss.name() );

		w = margin;
		h = 2*margin;

		int count;

		QListIterator<PsiIcon *> it = iss.iterator();
		count = 0;
		while ( it.hasNext() ) {
			if ( count++ >= displayNumIcons )
				break; // display only first displayNumIcons icons

			PsiIcon *icon = it.next();
			QPixmap pix = icon->pixmap();

			iconRects[icon] = QRect( w, margin, pix.width(), pix.height() );

			w += pix.width() + margin;
			h = qMax( h, pix.height() + 2*margin );

			connect(icon, SIGNAL(pixmapChanged()), SLOT(update()));
			icon->activated(false); // start animation
		}

		QMutableMapIterator<PsiIcon*, QRect> it2(iconRects);
		while (it2.hasNext()) {
			it2.next();
			QRect r = it2.value();
			it2.setValue(QRect(r.x(), (h - r.height()) / 2, r.width(), r.height()));
		}
#else
		Q_UNUSED( _iconset );
#endif
	}

	~IconsetSelectItem()
	{
#ifndef WIDGET_PLUGIN
		QMap<PsiIcon*, QRect>::Iterator it;
		for (it = iconRects.begin(); it != iconRects.end(); it++)
			it.key()->stop();
#endif
	}

	const Iconset *iconset() const
	{
#ifndef WIDGET_PLUGIN
		return &iss;
#else
		return 0;
#endif
	}
	
	int height() const
	{
		int hh = listWidget()->fontMetrics().lineSpacing() + h;
		return qMax( hh, QApplication::globalStrut().height() );
	}

	int width() const
	{
		int ww = qMax(listWidget()->fontMetrics().width( text() ) + 6 + 15, w + 10);
		return qMax( ww, QApplication::globalStrut().width() );
	}
	

	void paint(QPainter *painter) const
	{
#ifndef WIDGET_PLUGIN
		QFontMetrics fm = painter->fontMetrics();

		QMap<PsiIcon*, QRect>::ConstIterator it;
		for (it = iconRects.begin(); it != iconRects.end(); it++) {
			PsiIcon *icon = it.key();
			QRect r = it.value();
			painter->drawPixmap(QPoint(10 + r.left(), fm.lineSpacing() + 2 + r.top()), icon->pixmap());
		}
#else
		Q_UNUSED(painter);
#endif
	}
	QPoint textPosition(QPainter *painter) const
	{
		QFontMetrics fm = painter->fontMetrics();
		return QPoint(3, fm.ascent() + (fm.leading()+1)/2 + 1);
	}
};
const int IconsetSelectItem::margin = 3;
const int IconsetSelectItem::displayNumIcons = 10;

IconsetSelect::IconsetSelect(QWidget *parent)
: QListWidget(parent)
{
	setItemDelegate(new IconWidgetDelegate(this));
}

IconsetSelect::~IconsetSelect()
{
}

void IconsetSelect::insert(const Iconset &iconset)
{
#ifndef WIDGET_PLUGIN
	new IconsetSelectItem(this, iconset);
#else
	Q_UNUSED(iconset);
#endif
}

void IconsetSelect::moveItemUp()
{
	if ( currentRow() < 1 )
		return;

	QListWidgetItem *i = currentItem();
	if ( !i )
		return;
	int prevRow = row(i) - 1;
	i = takeItem(row(i));
	insertItem(prevRow, i);
	setItemSelected(i, true);
	setCurrentItem(i);
}

void IconsetSelect::moveItemDown()
{
	if ( !currentItem() || currentRow() > (int)count() - 2 )
		return;

	QListWidgetItem *i = currentItem();
	if ( !i )
		return;
	int nextRow = row(i) + 1;
	i = takeItem(row(i));
	insertItem(nextRow, i);
	setCurrentItem(i);
}

const Iconset *IconsetSelect::iconset() const
{
	IconsetSelectItem *i = (IconsetSelectItem *)currentItem();
	if ( !i ) {
		QList<QListWidgetItem *> items = selectedItems();
		i = !items.isEmpty() ? (IconsetSelectItem *)items.first() : 0;
	}
	if ( i )
		return i->iconset();
	return 0;
}

QListWidgetItem *IconsetSelect::lastItem() const
{
	return item(count() - 1);
}

QStyleOptionViewItem IconsetSelect::viewOptions() const
{
	QStyleOptionViewItem o = QListWidget::viewOptions();
	o.showDecorationSelected = true;
	return o;
}

//----------------------------------------------------------------------------
// IconsetDisplay
//----------------------------------------------------------------------------

class IconsetDisplayItem : public RealIconWidgetItem
{
	Q_OBJECT
private:
	static const int margin;
	PsiIcon icon;
	int w, h;

public:
	IconsetDisplayItem(QListWidget *parent, PsiIcon *i, int iconW)
	: RealIconWidgetItem(parent)
	{
#ifndef WIDGET_PLUGIN
		icon = *i;
		w = iconW;

		connect(&icon, SIGNAL(pixmapChanged()), SLOT(update()));
		icon.activated(false);

		h = icon.pixmap().height();

		QStringList str;
		foreach(PsiIcon::IconText t, icon.text())
			str += t.text;

		if ( !str.isEmpty() )
			setText(str.join(", "));
		else
			setText(tr("Name: '%1'").arg(icon.name()));
#else
		Q_UNUSED( i );
		Q_UNUSED( iconW );
#endif
	}

	~IconsetDisplayItem()
	{
#ifndef WIDGET_PLUGIN
		icon.stop();
#endif
	}

	int height() const
	{
		int hh = qMax(h + 2*margin, listWidget()->fontMetrics().lineSpacing() + 2);
		return qMax( hh, QApplication::globalStrut().height() );
	}

	int width() const
	{
		int ww = listWidget()->fontMetrics().width(text()) + w + 3*margin + 15;
		return qMax( ww, QApplication::globalStrut().width() );
	}

	void paint(QPainter *painter) const
	{
#ifndef WIDGET_PLUGIN
		painter->drawPixmap(QPoint((2*margin+w - icon.pixmap().width())/2, margin), icon.pixmap());
#else
		Q_UNUSED(painter);
#endif
	}
	QPoint textPosition(QPainter *painter) const
	{
		QFontMetrics fm = painter->fontMetrics();
		return QPoint(w + 2*margin, fm.ascent() + (fm.leading()+1)/2 + 1);
	}

};
const int IconsetDisplayItem::margin = 3;

IconsetDisplay::IconsetDisplay(QWidget *parent)
: QListWidget(parent)
{
	setItemDelegate(new IconWidgetDelegate(this));
}

IconsetDisplay::~IconsetDisplay()
{
}

void IconsetDisplay::setIconset(const Iconset &iconset)
{
#ifndef WIDGET_PLUGIN
	int w = 0;
	QListIterator<PsiIcon *> it = iconset.iterator();
	while ( it.hasNext() ) {
		w = qMax(w, it.next()->pixmap().width());
	}

	it = iconset.iterator();
	while ( it.hasNext() ) {
		new IconsetDisplayItem(this, it.next(), w);
	}
#else
	Q_UNUSED(iconset);
#endif
}

//----------------------------------------------------------------------------
// IconButton
//----------------------------------------------------------------------------

class IconButton::Private : public QObject
{
	Q_OBJECT
public:
	PsiIcon *icon;
	IconButton *button;
	bool textVisible;
	bool activate, forced;
#ifdef WIDGET_PLUGIN
	QString iconName;
#endif

public:
	Private(IconButton *b)
	{
		icon = 0;
		button = b;
		textVisible = true;
		forced = false;
	}

	~Private()
	{
		iconStop();
	}

	void setIcon(PsiIcon *i)
	{
#ifndef WIDGET_PLUGIN
		iconStop();
		if ( i )
			icon = i->copy();
		iconStart();
#else
		Q_UNUSED(i);
#endif
	}

	void iconStart()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			connect(icon, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
			if ( activate )
				icon->activated(true); // FIXME: should icon play sound when it's activated on button?
		}

		updateIcon();
#endif
	}

	void iconStop()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			disconnect(icon, 0, this, 0 );
			if ( activate )
				icon->stop();

			delete icon;
			icon = 0;
		}
#endif
	}

	void update()
	{
		iconUpdated();
	}

	void updateIcon()
	{
		iconUpdated();
	}

public slots:
	void iconUpdated()
	{
		button->setUpdatesEnabled(false);
#ifndef WIDGET_PLUGIN
		button->setIcon(icon ? icon->pixmap() : QPixmap());
#else
		QPixmap pix;
		if (!iconName.isEmpty()) {
			QPixmap pix((const char **)cancel_xpm);
			pix = QPixmap(pix);
		}
		button->setIcon(pix);
#endif
		button->setUpdatesEnabled(true);
		button->update();
	}
};

IconButton::IconButton(QWidget *parent)
: QPushButton(parent)
{
	d = new Private(this);
}

IconButton::~IconButton()
{
	delete d;
}

void IconButton::setIcon(const QPixmap &p)
{
	QPushButton::setIcon(p);
}

void IconButton::forceSetPsiIcon(const PsiIcon *i, bool activate)
{
	d->activate = activate;
	d->setIcon((PsiIcon *)i);
	d->forced = true;
}

void IconButton::setPsiIcon(const PsiIcon *i, bool activate)
{
#ifndef Q_WS_X11
	if ( !text().isEmpty() )
		return;
#endif

	forceSetPsiIcon(i, activate);
	d->forced = false;
}

void IconButton::setPsiIcon(const QString &name)
{
#ifndef WIDGET_PLUGIN
	setPsiIcon( IconsetFactory::iconPtr(name) );
#else
	d->iconName = name;
	d->iconUpdated();
#endif
}

QString IconButton::psiIconName() const
{
#ifndef WIDGET_PLUGIN
	if ( d->icon )
		return d->icon->name();
	return QString::null;
#else
	return d->iconName;
#endif
}

void IconButton::setText(const QString &text)
{
#ifndef Q_WS_X11
	if ( !d->forced )
		setPsiIcon(0);
#endif

	QPushButton::setText( text );
	d->updateIcon();
}

bool IconButton::textVisible() const
{
	return d->textVisible;
}

void IconButton::setTextVisible(bool v)
{
	d->textVisible = v;
	d->updateIcon();
}

//----------------------------------------------------------------------------
// IconToolButton
//----------------------------------------------------------------------------

class IconToolButton::Private : public QObject
{
	Q_OBJECT
public:
	PsiIcon *icon;
	IconToolButton *button;
	bool activate;
#ifdef WIDGET_PLUGIN
	QString iconName;
#endif

public:
	Private(IconToolButton *b)
	{
		icon = 0;
		button = b;
	}

	~Private()
	{
		iconStop();
	}

	void setIcon(const PsiIcon *i)
	{
#ifndef WIDGET_PLUGIN
		iconStop();
		if ( i )
			icon = new PsiIcon(*i);
		iconStart();
#else
		Q_UNUSED(i);
#endif
	}

	void iconStart()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			connect(icon, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
			if ( activate )
				icon->activated(true); // FIXME: should icon play sound when it's activated on button?
		}
		iconUpdated();
#endif
	}

	void iconStop()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			disconnect(icon, 0, this, 0 );
			if ( activate )
				icon->stop();

			delete icon;
			icon = 0;
		}
#endif
	}

	void update()
	{
		iconUpdated();
	}

private slots:
	void iconUpdated()
	{
		button->setUpdatesEnabled(false);
#ifndef WIDGET_PLUGIN
		QPixmap pix = icon ? icon->pixmap() : QPixmap();
		if (pix.isNull())
			button->QToolButton::setIcon(QIcon());
		else
			button->QToolButton::setIcon(pix);
		button->setIcon(icon ? icon->pixmap() : QPixmap());
#else
		button->setIcon(QPixmap());
#endif
		button->setUpdatesEnabled(true);
		button->update();
	}
};

IconToolButton::IconToolButton(QWidget *parent)
: QToolButton(parent)
{
	d = new Private(this);
}

IconToolButton::~IconToolButton()
{
	delete d;
}

void IconToolButton::setIcon(const QIcon &p)
{
	QToolButton::setIcon(p);
}

void IconToolButton::setPsiIcon(const PsiIcon *i, bool activate)
{
	d->activate = activate;
	d->setIcon ((PsiIcon *)i);
}

void IconToolButton::setPsiIcon(const QString &name)
{
#ifndef WIDGET_PLUGIN
	setPsiIcon( IconsetFactory::iconPtr(name) );
#else
	d->iconName = name;
#endif
}

QString IconToolButton::psiIconName() const
{
#ifndef WIDGET_PLUGIN
	if ( d->icon )
		return d->icon->name();
	return QString::null;
#else
	return d->iconName;
#endif
}

void IconToolButton::paintEvent(QPaintEvent* event)
{
	setToolButtonStyle(icon().isNull() ?
	                   Qt::ToolButtonTextOnly :
	                   Qt::ToolButtonIconOnly);
	QToolButton::paintEvent(event);
}

#include "iconwidget.moc"
