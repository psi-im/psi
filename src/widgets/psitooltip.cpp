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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psitooltip.h"

#include "private/qeffects_p.h"
#include "psitiplabel.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QHash>
#include <QKeyEvent>
#include <QPointer>
#include <QScreen>
#include <QToolTip>

//----------------------------------------------------------------------------
// PsiToolTipHandler
//----------------------------------------------------------------------------

class PsiToolTipHandler : public QObject {
    Q_OBJECT
public:
    static PsiToolTipHandler *getInstance()
    {
        if (!instance)
            instance = new PsiToolTipHandler();

        return instance;
    }

    void install(QWidget *widget)
    {
        if (!watchedWidgets_.contains(widget)) {
            connect(widget, SIGNAL(destroyed(QObject *)), SLOT(widgetDestroyed(QObject *)));
        }
        watchedWidgets_[widget] = true;
    }

private slots:
    void widgetDestroyed(QObject *obj)
    {
        QWidget *widget = static_cast<QWidget *>(obj);
        watchedWidgets_.remove(widget);
        if (watchedWidgets_.isEmpty()) {
            instance->deleteLater();
            instance = nullptr;
        }
    }

private:
    QHash<QWidget *, bool>    watchedWidgets_;
    static PsiToolTipHandler *instance;

    PsiToolTipHandler() : QObject(qApp) { qApp->installEventFilter(this); }

    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (event->type() == QEvent::ToolTip) {
            QWidget *widget = static_cast<QWidget *>(obj);
            if (watchedWidgets_.contains(widget)
                && (widget->isActiveWindow() || widget->window()->testAttribute(Qt::WA_AlwaysShowToolTips))) {
                QPoint pos = dynamic_cast<QHelpEvent *>(event)->globalPos();
                PsiToolTip::showText(pos, widget->toolTip(), widget);
                event->setAccepted(true);
                return true;
            }
        }

        return false;
    }
};

PsiToolTipHandler *PsiToolTipHandler::instance = nullptr;

//----------------------------------------------------------------------------
// ToolTipPosition
//----------------------------------------------------------------------------

ToolTipPosition::ToolTipPosition(const QPoint &_pos, const QWidget *_w) : pos(_pos), w(_w) {}

QPoint ToolTipPosition::calculateTipPosition(const QWidget *label) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QScreen *s;
    if (!(s = QApplication::screenAt(pos))) {
        qWarning("Failed to find screen for coords %dx%d", pos.x(), pos.y());
        return QPoint(0, 0);
    }
    QRect screen = s->geometry();
#else
    QRect screen = QApplication::desktop()->screenGeometry(pos);
#endif

    QPoint p = pos;
    p += QPoint(2,
#ifdef Q_OS_WIN
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

PsiToolTip::PsiToolTip() : QObject(QCoreApplication::instance()) {}

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

    bool preventAnimation = (PsiTipLabel::instance() != nullptr);

    installPsiToolTipFont();
    QFrame *label = createTipLabel(text, QApplication::desktop());
    label->move(calc->calculateTipPosition(label));

    if (QApplication::isEffectEnabled(Qt::UI_AnimateTooltip) == false || preventAnimation)
        label->show();
    else if (QApplication::isEffectEnabled(Qt::UI_FadeTooltip))
        qFadeEffect(label);
    else
        qScrollEffect(label);
}

bool PsiToolTip::moveAndUpdateTipLabel(PsiTipLabel *label, const QString &text) { return label->theText() == text; }

void PsiToolTip::updateTipLabel(PsiTipLabel *label, const QString &text)
{
    Q_UNUSED(label);
    Q_UNUSED(text);
}

ToolTipPosition *PsiToolTip::createTipPosition(const QPoint &cursorPos, const QWidget *parentWidget)
{
    return new ToolTipPosition(cursorPos, parentWidget);
}

PsiTipLabel *PsiToolTip::createTipLabel(const QString &text, QWidget *parent)
{
    PsiTipLabel *label = new PsiTipLabel(parent);
    label->init(text);
    return label;
}

/**
 * After installation, all tool tips in specified widget will be processed
 * through PsiToolTip and thus <icon> tags would be correctly handled.
 */
void PsiToolTip::doInstall(QWidget *w) { PsiToolTipHandler::getInstance()->install(w); }

PsiToolTip *PsiToolTip::instance_ = nullptr;

PsiToolTip *PsiToolTip::instance()
{
    if (!instance_)
        instance_ = new PsiToolTip();
    return instance_;
}

#include "psitooltip.moc"
