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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSITOOLTIP_H
#define PSITOOLTIP_H

#include <QWidget>

class PsiTipLabel;

class ToolTipPosition : public QObject {
public:
    ToolTipPosition(const QPoint &cursorPos, const QWidget *parentWidget);
    virtual ~ToolTipPosition() {}

    virtual QPoint calculateTipPosition(const QWidget *label) const;

protected:
    QPoint         pos;
    const QWidget *w;
};

class PsiToolTip : public QObject {
public:
    static void showText(const QPoint &pos, const QString &text, const QWidget *w = nullptr)
    {
        instance()->doShowText(pos, text, w);
    }
    static void install(QWidget *w) { instance()->doInstall(w); }

    static PsiToolTip *instance();

protected:
    PsiToolTip();
    void                     doShowText(const QPoint &pos, const QString &text, const QWidget *w = nullptr);
    void                     doInstall(QWidget *w);
    virtual ToolTipPosition *createTipPosition(const QPoint &cursorPos, const QWidget *parentWidget);
    virtual PsiTipLabel *    createTipLabel(const QString &text, QWidget *parent);
    virtual bool             moveAndUpdateTipLabel(PsiTipLabel *label, const QString &text);
    virtual void             updateTipLabel(PsiTipLabel *label, const QString &text);

    static PsiToolTip *instance_;
};

#endif // PSITOOLTIP_H
