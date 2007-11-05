/*
 * psitooltip.h - PsiIcon-aware QToolTip clone
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

#ifndef PSITOOLTIP_H
#define PSITOOLTIP_H

#include <QWidget>

class PsiTipLabel;

class ToolTipPosition : public QObject
{
public:
	ToolTipPosition(const QPoint& cursorPos, const QWidget* parentWidget);
	virtual ~ToolTipPosition() {}

	int getScreenNumber() const;
	QRect screenRect() const;

	virtual QPoint calculateTipPosition(const QWidget* label) const;

protected:
	QPoint pos;
	const QWidget* w;
};

class PsiToolTip : public QObject
{
public:
	static void showText(const QPoint &pos, const QString &text, const QWidget *w = 0) { instance()->doShowText(pos, text, w); }
	static void install(QWidget *w) { instance()->doInstall(w); }

	static PsiToolTip* instance();

protected:
	PsiToolTip();
	void doShowText(const QPoint &pos, const QString &text, const QWidget *w = 0);
	void doInstall(QWidget *w);
	virtual ToolTipPosition* createTipPosition(const QPoint& cursorPos, const QWidget* parentWidget);
	virtual PsiTipLabel* createTipLabel(const QString& text, QWidget* parent);
	virtual bool moveAndUpdateTipLabel(PsiTipLabel* label, const QString& text);
	virtual void updateTipLabel(PsiTipLabel* label, const QString& text);

	static PsiToolTip* instance_;
};

#endif
