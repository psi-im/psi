/*
 * thumbnailtoolbar.cpp - Thumbnail Toolbar Widget
 * Copyright (C) 2021  Vitaly Tonkacheyev
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

#include "thumbnailtoolbar.h"
#include "iconset.h"

#include <QWinThumbnailToolButton>
#include <QWindow>

PsiThumbnailToolBar::PsiThumbnailToolBar(QObject *parent, QWindow *parentWindow) :
    QWinThumbnailToolBar(parent), optionsBtn_(new QWinThumbnailToolButton(this)),
    onlineStatusBtn_(new QWinThumbnailToolButton(this)), offlineStatusBtn_(new QWinThumbnailToolButton(this)),
    eventsBtn_(new QWinThumbnailToolButton(this))
{
    // ToolTips
    optionsBtn_->setToolTip(tr("Options"));
    onlineStatusBtn_->setToolTip(tr("Online"));
    offlineStatusBtn_->setToolTip(tr("Offline"));
    eventsBtn_->setToolTip(tr("Show Next Event"));
    // Icons
    optionsBtn_->setIcon(IconsetFactory::iconPtr("psi/options")->icon());
    onlineStatusBtn_->setIcon(IconsetFactory::iconPtr("status/online")->icon());
    offlineStatusBtn_->setIcon(IconsetFactory::iconPtr("status/offline")->icon());
    eventsBtn_->setIcon(IconsetFactory::iconPtr("psi/events")->icon());
    // Set dismiss
    optionsBtn_->setDismissOnClick(true);
    onlineStatusBtn_->setDismissOnClick(true);
    offlineStatusBtn_->setDismissOnClick(true);
    eventsBtn_->setDismissOnClick(true);

    updateToolBar(false);

    connect(optionsBtn_, &QWinThumbnailToolButton::clicked, this, &PsiThumbnailToolBar::openOptions);
    connect(onlineStatusBtn_, &QWinThumbnailToolButton::clicked, this, &PsiThumbnailToolBar::setOnline);
    connect(offlineStatusBtn_, &QWinThumbnailToolButton::clicked, this, &PsiThumbnailToolBar::setOffline);
    connect(eventsBtn_, &QWinThumbnailToolButton::clicked, this, [this]() {
        if (eventsBtn_->isEnabled())
            emit runActiveEvent();
    });
    addButton(eventsBtn_);
    addButton(onlineStatusBtn_);
    addButton(offlineStatusBtn_);
    addButton(optionsBtn_);
    setWindow(parentWindow);
}

void PsiThumbnailToolBar::updateToolBar(bool hasEvents) { eventsBtn_->setEnabled(hasEvents); }
