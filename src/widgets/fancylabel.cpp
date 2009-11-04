/*
 * fancylabel.cpp - the FancyLabel widget
 * Copyright (C) 2003  Michail Pishchagin
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

#include "fancylabel.h"

#include <QPixmap>
#include <QColor>
#include <QLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QResizeEvent>

#ifndef WIDGET_PLUGIN
#include "iconset.h"
#endif

#include "iconlabel.h"

//----------------------------------------------------------------------------
// IconLabel
//----------------------------------------------------------------------------

class IconLabel::Private : public QObject
{
	Q_OBJECT

public:

	IconLabel *label;
	PsiIcon *icon;
	bool copyIcon;
#ifdef WIDGET_PLUGIN
	QString iconName;
#endif

	Private(IconLabel *l)
	{
		label = l;
		icon = 0;
	}

	~Private()
	{
		stopIcon();
#ifndef WIDGET_PLUGIN
		if ( icon && copyIcon )
			delete icon;
#endif
	}

	void setIcon(const PsiIcon *i, bool _copyIcon)
	{
		copyIcon = _copyIcon;

		stopIcon();
#ifndef WIDGET_PLUGIN
		if ( i && copyIcon )
			icon = new PsiIcon(*i);
		else
			icon = (PsiIcon *)i;
#else
    Q_UNUSED(i);
#endif
		startIcon();
	}

protected:
	void stopIcon()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			disconnect(icon, 0, this, 0);
			icon->stop();
		}
#endif
	}

	void startIcon()
	{
#ifndef WIDGET_PLUGIN
		if ( icon ) {
			connect(icon, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
			icon->activated(false); // TODO: should icon play sound when it's activated on icon?
		}
		iconUpdated();
#endif
	}

private slots:
	void iconUpdated()
	{
#ifndef WIDGET_PLUGIN
		label->setPixmap(icon ? icon->pixmap() : QPixmap());
#endif
	}
};

IconLabel::IconLabel(QWidget *parent)
: QLabel(parent)
{
	d = new Private(this);
}

IconLabel::~IconLabel()
{
	delete d;
}

const PsiIcon *IconLabel::psiIcon () const
{
	return d->icon;
}

QString IconLabel::psiIconName () const
{
#ifndef WIDGET_PLUGIN
	if ( d->icon )
		return d->icon->name();
	return QString::null;
#else
	return d->iconName;
#endif
}

void IconLabel::setPsiIcon(const PsiIcon *i, bool copyIcon)
{
	d->setIcon(i, copyIcon);
}

void IconLabel::setPsiIcon(const QString &name)
{
#ifndef WIDGET_PLUGIN
	setPsiIcon( IconsetFactory::iconPtr(name) );
#else
	d->iconName = name;
	setText("<qt>icon:<br><small>" + name + "</small></qt>");
#endif
}

void IconLabel::setScaledContents(int width, int height)
{
	QLabel::setScaledContents(true);
	setMinimumSize(width, height);
	setMaximumSize(width, height);
}

//----------------------------------------------------------------------------
// MyFancyFrame -- internal
//----------------------------------------------------------------------------

class MyFancyFrame : public QFrame
{
	Q_OBJECT
protected:
	void paintEvent(QPaintEvent *event)
	{
		QPainter *p = new QPainter(this);
		p->drawPixmap(0, 0, background);
		delete p;
		QFrame::paintEvent(event);
	}

	void resizeEvent (QResizeEvent *e)
	{
		QFrame::resizeEvent (e);

		QRect rect = geometry();
		int w = rect.width();

		if ( rect.height() <= 0 || w <= 0 )
			return; // avoid crash

		int r1, g1, b1;
		from->getRgb (&r1, &g1, &b1);
		int r2, g2, b2;
		to->getRgb (&r2, &g2, &b2);

		float stepR = (float)(r2 - r1) / w;
		float stepG = (float)(g2 - g1) / w;
		float stepB = (float)(b2 - b1) / w;

		QPixmap pix (rect.width(), rect.height());
		QPainter p;
		p.begin (&pix);
		for (int i = 0; i < w; i++) {
			int r = (int)((float)r1 + stepR*i);
			int g = (int)((float)g1 + stepG*i);
			int b = (int)((float)b1 + stepB*i);

			p.setPen ( QColor( r, g, b ) );
			p.drawLine ( i, 0, i, rect.height() );
		}
		p.end ();

		background = pix;
		update();
	}

private:
	QColor *from, *to;
	QPixmap background;

public:
	MyFancyFrame (QWidget *parent, QColor *_from, QColor *_to)
	: QFrame (parent)
	{
		from = _from;
		to = _to;
	}
	
	void repaintBackground()
	{
		QResizeEvent e(size(), size());
		resizeEvent(&e);
	}
};

//----------------------------------------------------------------------------
// FancyLabel
//----------------------------------------------------------------------------

class FancyLabel::Private : public QObject
{
public:
	MyFancyFrame *frame;
	IconLabel *text, *help, *pix;
	QColor from, to, font;
	QString textStr, helpStr;
	static int smallFontSize;

	Private (FancyLabel *parent)
	: QObject(parent), from(72, 172, 243), to(255, 255, 255), font(0, 0, 0)
	{
		QHBoxLayout *mainbox = new QHBoxLayout( parent );
		mainbox->setSpacing(0);
		mainbox->setMargin(0);

		frame = new MyFancyFrame ( parent, &from, &to );
		frame->setFrameShape( QFrame::StyledPanel );
		frame->setFrameShadow( QFrame::Raised );

		QHBoxLayout *frameLayout = new QHBoxLayout(frame);
		frameLayout->setMargin(0);
		frameLayout->setSpacing(0);
		QVBoxLayout *layout = new QVBoxLayout;
		layout->setMargin(0);
		layout->setSpacing(0);
		frameLayout->addLayout( layout );

		text = new IconLabel( frame );
		text->setSizePolicy( (QSizePolicy::Policy)7, (QSizePolicy::Policy)5 );
		layout->addWidget( text );

		help = new IconLabel( frame );
		layout->addWidget( help );

		QFont font = help->font();
		font.setPointSize(smallFontSize);
		help->setFont(font);

		pix = new IconLabel( frame );
		pix->setSizePolicy( (QSizePolicy::Policy)1, (QSizePolicy::Policy)5 );
		frameLayout->addWidget( pix );

		mainbox->addWidget( frame );
	}
};

int FancyLabel::Private::smallFontSize = 0;

FancyLabel::FancyLabel (QWidget *parent)
: QWidget (parent)
{
	d = new Private (this);
}

FancyLabel::~FancyLabel ()
{
}

void FancyLabel::setText (const QString &text)
{
	d->textStr = text;
	d->text->setText (QString("<font color=\"%1\"><b>").arg(d->font.name()) + text + "</b></font>");
}

void FancyLabel::setHelp (const QString &help)
{
	d->helpStr = help;

	QString f1 = "<small>";
	QString f2 = "</small>";
	if ( d->smallFontSize ) {
		f1 = "<font>";
		f2 = "</font>";
	}

	d->help->setText (QString("<font color=\"%1\">").arg(d->font.name()) + f1 + help + f2 + "</font>");
}

void FancyLabel::setPixmap (const QPixmap &pix)
{
	d->pix->setPixmap( pix );
}

void FancyLabel::setColorFrom (const QColor &col)
{
	d->from = col;
	d->frame->repaintBackground();
}

void FancyLabel::setColorTo (const QColor &col)
{
	d->to = col;
	d->frame->repaintBackground();
}

void FancyLabel::setColorFont (const QColor &col)
{
	d->font = col;
	d->frame->repaintBackground();
}

const QString &FancyLabel::text () const
{
	return d->textStr;
}

const QString &FancyLabel::help () const
{
	return d->helpStr;
}

const QPixmap *FancyLabel::pixmap () const
{
	return d->pix->pixmap();
}

const QColor &FancyLabel::colorFrom () const
{
	return d->from;
}

const QColor &FancyLabel::colorTo () const
{
	return d->to;
}

const QColor &FancyLabel::colorFont () const
{
	return d->font;
}

const PsiIcon *FancyLabel::psiIcon () const
{
	return d->pix->psiIcon();
}

void FancyLabel::setPsiIcon (const PsiIcon *i)
{
	d->pix->setPsiIcon(i);
}

void FancyLabel::setPsiIcon (const QString &name)
{
	d->pix->setPsiIcon(name);
}

QString FancyLabel::psiIconName () const
{
	return d->pix->psiIconName();
}

FancyLabel::Shape FancyLabel::frameShape () const
{
	return (FancyLabel::Shape)(int)d->frame->frameShape();
}

void FancyLabel::setFrameShape (FancyLabel::Shape v)
{
	d->frame->setFrameShape( (QFrame::Shape)(int)v );
	d->frame->repaintBackground();
}

FancyLabel::Shadow FancyLabel::frameShadow () const
{
	return (FancyLabel::Shadow)(int)d->frame->frameShadow();
}

void FancyLabel::setFrameShadow (FancyLabel::Shadow v)
{
	d->frame->setFrameShadow( (QFrame::Shadow)(int)v );
	d->frame->repaintBackground();
}

int FancyLabel::lineWidth () const
{
	return d->frame->lineWidth();
}

void FancyLabel::setLineWidth (int v)
{
	d->frame->setLineWidth(v);
	d->frame->repaintBackground();
}

int FancyLabel::midLineWidth () const
{
	return d->frame->midLineWidth();
}

void FancyLabel::setMidLineWidth (int v)
{
	d->frame->setMidLineWidth(v);
	d->frame->repaintBackground();
}

void FancyLabel::setScaledContents(int width, int height)
{
	d->pix->setScaledContents(width, height);
}

void FancyLabel::setSmallFontSize(int s)
{
	Private::smallFontSize = s;
}

#include "fancylabel.moc"
