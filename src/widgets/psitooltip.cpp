/*
 * psitooltip.cpp - PsiIcon-aware QToolTip clone
 * Copyright (C) 2006  Michail Pishchagin
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

#include "psitooltip.h"

#include <QFrame>
#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QHash>
#include <QPointer>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QToolTip>
#include <QAbstractTextDocumentLayout>
#include <QTextFrame>
#include <QTextEdit>
#include "private/qeffects_p.h"

#include "psirichtext.h"

//----------------------------------------------------------------------------
// PsiTipLabel
//----------------------------------------------------------------------------

class PsiTipLabel : public QFrame
{
	Q_OBJECT
public:
	PsiTipLabel(const QString& text, QWidget* parent);
	~PsiTipLabel();
	static PsiTipLabel *instance;

	// int heightForWidth(int w) const;
	QSize sizeHint() const;
	QSize minimumSizeHint() const;
	bool eventFilter(QObject *, QEvent *);

	QString theText() const;

	QBasicTimer hideTimer, deleteTimer;

	void hideTip();
protected:
	void enterEvent(QEvent*){ hideTip(); }
	void timerEvent(QTimerEvent *e);
	void paintEvent(QPaintEvent *e);
	// QSize sizeForWidth(int w) const;

private:
	QTextDocument *doc;
	QString theText_;
	bool isRichText;
	int margin;
	// int indent;
};

PsiTipLabel *PsiTipLabel::instance = 0;

PsiTipLabel::PsiTipLabel(const QString& text, QWidget* parent)
	: QFrame(parent, Qt::ToolTip)
{
	delete instance;
	instance = this;

	margin = 1 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, 0, this);
	setFrameStyle(QFrame::NoFrame);

	// doc = new QTextDocument(this);
	// QTextDocumentLayout is private in Qt4
	// and it's impossible to set wrapping mode directly.
	// So we create this QTextEdit instance and use its QTextDocument,
	// just because QTextEdit can set the wrapping mode.
	// Yes, this is crazy...
	QTextEdit *edit = new QTextEdit(this);
	edit->hide();
	edit->setWordWrapMode(QTextOption::WordWrap);
	doc = edit->document();
	doc->setUndoRedoEnabled(false);
	doc->setDefaultFont(font());

	ensurePolished();
	
	theText_ = text;
	isRichText = false;
	if (Qt::mightBeRichText(theText_)) {
		isRichText = true;
		PsiRichText::install(doc);
		PsiRichText::setText(doc, theText_);
	}
	else {
		doc->setPlainText(theText_);
	}

	resize(sizeHint());
	qApp->installEventFilter(this);
	hideTimer.start(10000, this);
	setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, 0, this) / 255.0);
	// No resources for this yet (unlike on Windows).
	//QPalette pal(Qt::black, QColor(255,255,220),
	//             QColor(96,96,96), Qt::black, Qt::black,
	//             Qt::black, QColor(255,255,220));
	setPalette(QToolTip::palette());
}

QString PsiTipLabel::theText() const
{
	return theText_;
}

/*
QSize PsiTipLabel::sizeForWidth(int w) const
{
	QRect br;
	
	int hextra = 2 * margin;
	int vextra = hextra;
	
	if (isRichText) {
		hextra = 1;
		vextra = 1;
	}
	
	PsiRichText::ensureTextLayouted(doc, w);
	const qreal oldTextWidth = doc->textWidth();
	
	doc->adjustSize();
	br = QRect(QPoint(0, 0), doc->size().toSize());
	doc->setTextWidth(oldTextWidth);
	
	QFontMetrics fm(font());
	QSize extra(hextra + 1, vextra);

	// Make it look good with the default ToolTip font on Mac, which has a small descent.
	if (fm.descent() == 2 && fm.ascent() >= 11)
		vextra++;
	
	const QSize contentsSize(br.width() + hextra, br.height() + vextra);
	return contentsSize;
}
*/
QSize PsiTipLabel::sizeHint() const
{
	QTextFrameFormat fmt = doc->rootFrame()->frameFormat();
	fmt.setMargin(0);
	doc->rootFrame()->setFrameFormat(fmt);
	// PsiRichText::ensureTextLayouted(doc, -1);

	doc->adjustSize();
	// br = QRect(QPoint(0, 0), doc->size().toSize());
	// this way helps to fight empty space on the right:
	QSize docSize = QSize(doc->idealWidth(), doc->size().toSize().height());

	QFontMetrics fm(font());
	QSize extra(2*margin + 2, 2*margin + 1);	// "+" for tip's frame
	// Make it look good with the default ToolTip font on Mac, which has a small descent.
	if (fm.descent() == 2 && fm.ascent() >= 11)
		++extra.rheight();

	return docSize + extra;
}

QSize PsiTipLabel::minimumSizeHint() const
{
	return sizeHint();
	// qWarning("PsiTipLabel::minimumSizeHint");
	// ensurePolished();
	// QSize sh = sizeForWidth(-1);
	// QSize msh(-1, -1);
	// 
	// msh.rheight() = sizeForWidth(QWIDGETSIZE_MAX).height(); // height for one line
	// msh.rwidth()  = sizeForWidth(0).width(); // wrap ? size of biggest word : min doc size
	// if (sh.height() < msh.height())
	// 	msh.rheight() = sh.height();
	// 
	// return msh;
}

void PsiTipLabel::paintEvent(QPaintEvent *)
{
	QStylePainter p(this);
	QStyleOptionFrame opt;
	opt.init(this);
	p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
	p.end();

	// stolen from QLabel::paintEvent
	QPainter painter(this);
	drawFrame(&painter);
	QRect cr = contentsRect();
	cr.adjust(margin, margin, -margin, -margin);

	PsiRichText::ensureTextLayouted(doc, width() - 2*margin);
	QAbstractTextDocumentLayout *layout = doc->documentLayout();
	// QRect lr = rect();
	QRect lr = cr;

	QAbstractTextDocumentLayout::PaintContext context;

	// Adjust the palette
	context.palette = palette();
	if (foregroundRole() != QPalette::Text && isEnabled())
		context.palette.setColor(QPalette::Text, context.palette.color(foregroundRole()));

	painter.save();
	painter.translate(lr.x() + 1, lr.y() + 1);
	painter.setClipRect(lr.translated(-lr.x() - 1, -lr.y() - 1));
	layout->draw(&painter, context);
	painter.restore();
}

PsiTipLabel::~PsiTipLabel()
{
	instance = 0;
}

void PsiTipLabel::hideTip()
{
	hide();
	// timer based deletion to prevent animation
	deleteTimer.start(250, this);
}

void PsiTipLabel::timerEvent(QTimerEvent *e)
{
	if (e->timerId() == hideTimer.timerId())
		hideTip();
	else if (e->timerId() == deleteTimer.timerId())
		delete this;
}

bool PsiTipLabel::eventFilter(QObject *, QEvent *e)
{
	switch (e->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease: {
			int key = static_cast<QKeyEvent *>(e)->key();
			Qt::KeyboardModifiers mody = static_cast<QKeyEvent *>(e)->modifiers();

			if ((mody & Qt::KeyboardModifierMask)
			    || (key == Qt::Key_Shift || key == Qt::Key_Control
			    || key == Qt::Key_Alt || key == Qt::Key_Meta))
				break;
		}
		case QEvent::Leave:
		case QEvent::WindowActivate:
		case QEvent::WindowDeactivate:
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::FocusIn:
		case QEvent::FocusOut:
		case QEvent::Wheel:
			hideTip();
		default:
			;
	}
	return false;
}

//----------------------------------------------------------------------------
// PsiToolTipHandler
//----------------------------------------------------------------------------

class PsiToolTipHandler : public QObject
{
	Q_OBJECT
public:
	static PsiToolTipHandler *getInstance()
	{
		static PsiToolTipHandler *instance = 0;
		if (!instance)
			instance = new PsiToolTipHandler();
			
		return instance;
	}

	void install(QWidget *widget)
	{
		widget->installEventFilter(this);
	}
	
private:
	PsiToolTipHandler()
		: QObject(qApp)
	{ }

	bool eventFilter(QObject *obj, QEvent *event)
	{
		if (event->type() == QEvent::ToolTip) {
			QWidget *widget = dynamic_cast<QWidget *>(obj);
			if (widget->isActiveWindow()) {
				QPoint pos = dynamic_cast<QHelpEvent *>(event)->globalPos();
				PsiToolTip::showText(pos, widget->toolTip(), widget);
				event->setAccepted(true);
				return true;
			}
		}

		return QObject::eventFilter(obj, event);
	}
};

//----------------------------------------------------------------------------
// ToolTipPosition
//----------------------------------------------------------------------------

class ToolTipPosition
{
private:
	QPoint pos;
	QWidget *w;
	
public:
	ToolTipPosition(const QPoint &_pos, QWidget *_w)
	{
		pos = _pos;
		w   = _w;
	}

	int getScreenNumber()
	{
		if (QApplication::desktop()->isVirtualDesktop())
			return QApplication::desktop()->screenNumber(pos);
		
		return QApplication::desktop()->screenNumber(w);
	}

	QPoint calculateTipPosition(QWidget *label)
	{
#ifdef Q_WS_MAC
		QRect screen = QApplication::desktop()->availableGeometry(getScreenNumber());
#else
		QRect screen = QApplication::desktop()->screenGeometry(getScreenNumber());
#endif

		QPoint p = pos;
		p += QPoint(2,
#ifdef Q_WS_WIN
		              24
#else
		              16
#endif
		           );

		if (p.x() + label->width() > screen.x() + screen.width())
			p.rx() -= 4 + label->width();
		if (p.y() + label->height() > screen.y() + screen.height())
			p.ry() -= 24 + label->height();
		if (p.y() < screen.y())
			p.setY(screen.y());
		if (p.x() + label->width() > screen.x() + screen.width())
			p.setX(screen.x() + screen.width() - label->width());
		if (p.x() < screen.x())
			p.setX(screen.x());
		if (p.y() + label->height() > screen.y() + screen.height())
			p.setY(screen.y() + screen.height() - label->height());
	
		return p;
	}
};

//----------------------------------------------------------------------------
// PsiToolTip
//----------------------------------------------------------------------------

/**
 * QTipLabel's font is being determined at run-time. However QTipLabel's and
 * QToolTip's font is the same, so we install our PsiTextLabel's font to be
 * the same as QToolTip's.
 */
static void installPsiToolTipFont()
{
	static bool toolTipFontInstalled = false;
	if (toolTipFontInstalled)
		return;

	qApp->setFont(QToolTip::font(), "PsiTipLabel");

	toolTipFontInstalled = true;
}

/**
 * Shows \a text as a tool tip, at global position \a pos. The
 * optional widget argument, \a w, is used to determine the
 * appropriate screen on multi-head systems.
 */
void PsiToolTip::showText(const QPoint &pos, const QString &text, QWidget *w)
{
	if (text.isEmpty()) {
		if (PsiTipLabel::instance)
			PsiTipLabel::instance->hideTip();
		return;
	}

	if (!w->underMouse())
		return;

	ToolTipPosition calc(pos, w);
	if (PsiTipLabel::instance && PsiTipLabel::instance->theText() == text) {
		// fancy moving tooltip effect
		PsiTipLabel::instance->move(calc.calculateTipPosition(PsiTipLabel::instance));
		return;
	}

	bool preventAnimation = (PsiTipLabel::instance != 0);

	installPsiToolTipFont();
	QFrame *label = new PsiTipLabel(text, QApplication::desktop()->screen(calc.getScreenNumber()));
	label->move(calc.calculateTipPosition(label));

	if ( QApplication::isEffectEnabled(Qt::UI_AnimateTooltip) == false || preventAnimation)
		label->show();
	else if (QApplication::isEffectEnabled(Qt::UI_FadeTooltip))
		qFadeEffect(label);
	else
		qScrollEffect(label);
}

/**
 * After installation, all tool tips in specified widget will be processed
 * through PsiToolTip and thus <icon> tags would be correctly handled.
 */
void PsiToolTip::install(QWidget *w)
{
	PsiToolTipHandler::getInstance()->install(w);
}

#include "psitooltip.moc"
