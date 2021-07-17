/*
 * thumbnailtoolbar.h - Thumbnail Toolbar Widget
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
#ifndef THUMBNAIL_TOOLBAR_H
#define THUMBNAIL_TOOLBAR_H

#include <QObject>
#include <QWinTaskbarButton>
#include <QWinThumbnailToolBar>

class QWindow;

class PsiThumbnailToolBar : public QWinThumbnailToolBar {
    Q_OBJECT
public:
    explicit PsiThumbnailToolBar(QObject *parent = 0, QWindow *parentWindow = 0);
    void updateToolBar(bool hasEvents);

signals:
    void openOptions();
    void setOnline();
    void setOffline();
    void runActiveEvent();

private:
    QWinThumbnailToolButton *optionsBtn_;
    QWinThumbnailToolButton *onlineStatusBtn_;
    QWinThumbnailToolButton *offlineStatusBtn_;
    QWinThumbnailToolButton *eventsBtn_;
    QWinTaskbarButton *      taskbarBtn_;
};
#endif // THUMBNAIL_TOOLBAR_H
