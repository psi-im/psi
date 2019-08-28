/*
 * psitabwidget.h - Customised QTabWidget for Psi
 * Copyright (C) 2006  Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSITABWIDGET_H
#define PSITABWIDGET_H

#include "psitabbar.h"

#include <QDragEnterEvent>
#include <QTabWidget>

#define PSITABDRAGMIMETYPE "x-drag-drop/x-psi-tab-drag"

class QHBoxLayout;
class QMenu;
class QStackedLayout;
class QToolButton;
class QVBoxLayout;

/**
 * \class PsiTabWidget
 * \brief
 */
class PsiTabWidget : public QWidget //: public QTabWidget
{
    Q_OBJECT
public:
    PsiTabWidget(QWidget *parent = nullptr);
    ~PsiTabWidget();

    void setTabTextColor(QWidget *tab, const QColor &color);
    int count();
    QWidget *currentPage();
    int currentPageIndex();
    QWidget *widget(int index);
    void addTab(QWidget *, QString, const QIcon &icon = QIcon());
    void showPage(QWidget *);
    void showPageDirectly(QWidget *);

    void setPagePinned(QWidget *page, bool pinned);
    bool isPagePinned(QWidget *page);

    void removePage(QWidget *);
    QWidget* page(int index);
    int getIndex(QWidget *);
    void setTabText(QWidget *, const QString &);
    void setTabIcon(QWidget *, const QIcon &);
    void setTabPosition(QTabWidget::TabPosition pos);
    void setCloseIcon(const QIcon &);

    void setTabBarShown(bool shown);     // default shown
    void setTabButtonsShown(bool shown); // default shown
    void setDragsEnabled(bool enabled);  // default enabled

    void setTabBarUpdateEnabled(bool b);

public slots:
    void setCurrentPage(int);
    void removeCurrentPage();
    void setLooks();

signals:
    void mouseDoubleClickTab(QWidget *tab);
    void mouseMiddleClickTab(QWidget *tab);
    void currentChanged(QWidget *selected);
    void closeButtonClicked();
    void aboutToShowMenu(QMenu *);
    void tabCloseRequested(int i);

    // context menu on the blank space will have tab==-1
    void tabContextMenu(int tab, QPoint pos, QContextMenuEvent *event);

protected:
    void resizeEvent(QResizeEvent *event);

private slots:
    void mouseDoubleClickTab(int tab);
    void mouseMiddleClickTab(int tab);
    void tab_currentChanged(int tab);
    void tab_contextMenu(QContextMenuEvent *event, int tab);
    void menu_aboutToShow();
    void menu_triggered(QAction *act);
    void widgetMoved(int from, int to);

private:
    QVector<QWidget*> widgets_;
    PsiTabBar *tabBar_;
    QVBoxLayout *layout_;
    QHBoxLayout *barLayout_;
    QStackedLayout *stacked_;
    QToolButton *closeButton_;
    QToolButton *downButton_;
    QTabWidget::TabPosition tabsPosition_;
    QMenu *menu_;
};

#endif // PSITABWIDGET_H
