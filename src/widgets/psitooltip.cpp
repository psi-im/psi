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

#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QPointer>
#include <QHash>
#include <QToolTip>
#include "private/qeffects_p.h"

#include "psitiplabel.h"

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
		watchedWidgets_[widget] = true;
		connect(widget, SIGNAL(destroyed(QObject*)), SLOT(widgetDestroyed(QObject*)));
	}

private slots:
	void widgetDestroyed(QObject* obj)
	{
		QWidget* widget = static_cast<QWidget*>(obj);
		watchedWidgets_.remove(widget);
	}

private:
	QHash<QWidget*, bool> watchedWidgets_;

	PsiToolTipHandler()
		: QObject(qApp)
	{
		qApp->installEventFilter(this);
	}

	bool eventFilter(QObject *obj, QEvent *event)
	{
		if (event->type() == QEvent::ToolTip) {
			QWidget *widget = static_cast<QWidget *>(obj);
			if (watchedWidgets_.contains(widget) &&
			    (widget->isActiveWindow() ||
			     widget->window()->testAttribute(Qt::WA_AlwaysShowToolTips))) {
				QPoint pos = dynamic_cast<QHelpEvent *>(event)->globalPos();
				PsiToolTip::showText(pos, widget->toolTip(), widget);
				event->setAccepted(true);
				return true;
			}
		}

		return false;
	}
};

//----------------------------------------------------------------------------
// ToolTipPosition
//----------------------------------------------------------------------------

ToolTipPosition::ToolTipPosition(const QPoint& _pos, const QWidget* _w)
	: pos(_pos)
	, w(_w)
{
}

int ToolTipPosition::getScreenNumber() const
{
	if (QApplication::desktop()->isVirtualDesktop())
		return QApplication::desktop()->screenNumber(pos);

	return QApplication::desktop()->screenNumber(w);
}

QRect ToolTipPosition::screenRect() const
{
#ifdef Q_WS_MAC
	return QApplication::desktop()->availableGeometry(getScreenNumber());
#else
	return QApplication::desktop()->screenGeometry(getScreenNumber());
#endif
}

QPoint ToolTipPosition::calculateTipPosition(const QWidget* label) const
{
	QRect screen = screenRect();

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

//----------------------------------------------------------------------------
// PsiToolTip
//----------------------------------------------------------------------------

PsiToolTip::PsiToolTip()
	: QObject(QCoreApplication::instance())
{}

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
void PsiToolTip::doShowText(const QPoint &pos, const QString &text, const QWidget *w)
{
	if (text.isEmpty() || (w && !w->underMouse())) {
		if (PsiTipLabel::instance())
			PsiTipLabel::instance()->hideTip();
		return;
	}

	QPointer<ToolTipPosition> calc(createTipPosition(pos, w));
	calc->deleteLater();
	if (PsiTipLabel::instance() && moveAndUpdateTipLabel(PsiTipLabel::instance(), text)) {
		updateTipLabel(PsiTipLabel::instance(), text);

		// fancy moving tooltip effect
		PsiTipLabel::instance()->move(calc->calculateTipPosition(PsiTipLabel::instance()));
		return;
	}

	bool preventAnimation = (PsiTipLabel::instance() != 0);

	installPsiToolTipFont();
	QFrame *label = createTipLabel(text, QApplication::desktop()->screen(calc->getScreenNumber()));
	label->move(calc->calculateTipPosition(label));

	if ( QApplication::isEffectEnabled(Qt::UI_AnimateTooltip) == false || preventAnimation)
		label->show();
	else if (QApplication::isEffectEnabled(Qt::UI_FadeTooltip))
		qFadeEffect(label);
	else
		qScrollEffect(label);
}

bool PsiToolTip::moveAndUpdateTipLabel(PsiTipLabel* label, const QString& text)
{
	return label->theText() == text;
}

void PsiToolTip::updateTipLabel(PsiTipLabel* label, const QString& text)
{
	Q_UNUSED(label);
	Q_UNUSED(text);
}

ToolTipPosition* PsiToolTip::createTipPosition(const QPoint& cursorPos, const QWidget* parentWidget)
{
	return new ToolTipPosition(cursorPos, parentWidget);
}

PsiTipLabel* PsiToolTip::createTipLabel(const QString& text, QWidget* parent)
{
	PsiTipLabel* label = new PsiTipLabel(parent);
	label->init(text);
	return label;
}

/**
 * After installation, all tool tips in specified widget will be processed
 * through PsiToolTip and thus <icon> tags would be correctly handled.
 */
void PsiToolTip::doInstall(QWidget *w)
{
	PsiToolTipHandler::getInstance()->install(w);
}

PsiToolTip *PsiToolTip::instance_ = 0;

PsiToolTip* PsiToolTip::instance()
{
	if (!instance_)
		instance_ = new PsiToolTip();
	return instance_;
}

#include "psitooltip.moc"
