/*
 * tabbar.h
 * Copyright (C) 2013-2014  Ivan Romanov <drizt@land.ru>
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

#pragma once

#include <QTabBar>

class TabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit TabBar(QWidget *parent = nullptr);
    ~TabBar();

    void layoutTabs();

    void setMultiRow(bool b);
    bool multiRow() const;

    void setDragsEnabled(bool enabled); // default enabled
    void setTabPinned(int index, bool pinned);
    bool isTabPinned(int index);

    void setCurrentIndexAlwaysAtBottom(bool b);
    bool currentIndexAlwaysAtBottom() const;

    // reimplemented
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    QSize tabSizeHint(int index) const;
    void setTabsClosable(bool b);
    bool tabsClosable() const;
    void setCurrentIndex(int index);
    void setTabText(int index, const QString &text);
    void setTabTextColor(int index, const QColor &color);
    void setTabIcon(int index, const QIcon &icon);
    QRect tabRect(int index) const;
    QWidget *tabButton(int index, ButtonPosition position) const;
    int tabAt(const QPoint &position) const;
    bool eventFilter(QObject *watched, QEvent *event);
    void setUpdateEnabled(bool b);

protected:
    // reimplemented
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

    void dragMoveEvent(QDragMoveEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    void leaveEvent(QEvent *event);
    void tabInserted(int index);
    void tabRemoved(int index);

private slots:
    void closeTab();

private:
    class Private;
    Private *d;
};
