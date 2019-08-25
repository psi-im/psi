/*
 * psitabwidget.cpp - Customised QTabWidget for Psi
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

#include "psitabwidget.h"

#include "common.h"
#include "psioptions.h"
#include "psitabbar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QStackedLayout>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

/**
 * Constructor
 */
PsiTabWidget::PsiTabWidget(QWidget *parent)
        : QWidget(parent) {
    tabsPosition_ = QTabWidget::East; // impossible => uninitialised state
    tabBar_ = new PsiTabBar(this);
    bool multiRow = PsiOptions::instance()->getOption("options.ui.tabs.multi-rows", true).toBool();
    bool currentIndexAlwaysAtBottom  = PsiOptions::instance()->getOption("options.ui.tabs.current-index-at-bottom", true).toBool();
    tabBar_->setMultiRow(multiRow);
    tabBar_->setUsesScrollButtons(!multiRow);
    tabBar_->setCurrentIndexAlwaysAtBottom(currentIndexAlwaysAtBottom);
    layout_ = new QVBoxLayout(this);
    layout_->setMargin(0);
    layout_->setSpacing(0);
    barLayout_ = new QHBoxLayout;
    layout_->addLayout(barLayout_);
    barLayout_->setMargin(0);
    barLayout_->setSpacing(0);
    barLayout_->addWidget(tabBar_, 2);
    barLayout_->setAlignment(Qt::AlignLeft);

    int buttonwidth = qMax(tabBar_->style()->pixelMetric(QStyle::PM_TabBarScrollButtonWidth, nullptr, tabBar_),
        QApplication::globalStrut().width());

    downButton_ = new QToolButton(this);
    downButton_->setMinimumSize(3,3);
    downButton_->setFixedWidth(buttonwidth);
    downButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    menu_ = new QMenu(this);
    downButton_->setMenu(menu_);
    downButton_->setStyleSheet(" QToolButton::menu-indicator { image:none } ");
    connect(menu_, SIGNAL(aboutToShow()), SLOT(menu_aboutToShow()));
    connect(menu_, SIGNAL(triggered(QAction*)), SLOT(menu_triggered(QAction*)));
    barLayout_->addWidget(downButton_);
    barLayout_->setAlignment(downButton_, Qt::AlignBottom);

    closeButton_ = new QToolButton(this);
    closeButton_->setMinimumSize(3,3);
    closeButton_->setFixedWidth(buttonwidth);
    closeButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    barLayout_->addWidget(closeButton_);
    barLayout_->setAlignment(closeButton_, Qt::AlignBottom);
    closeButton_->setText("x");
    downButton_->setArrowType(Qt::DownArrow);
    downButton_->setPopupMode(QToolButton::InstantPopup);
    stacked_ = new QStackedLayout(layout_);

    setTabPosition(QTabWidget::North);
    setLooks();

    if (!PsiOptions::instance()->getOption("options.ui.tabs.show-tab-buttons").toBool()){
        closeButton_->hide();
        downButton_->hide();
    }
    if (!PsiOptions::instance()->getOption("options.ui.tabs.show-tab-close-buttons").toBool()){
        tabBar_->setTabsClosable(false);
    }

    connect( tabBar_, SIGNAL(mouseDoubleClickTab(int)), SLOT(mouseDoubleClickTab(int)));
    connect( tabBar_, SIGNAL(mouseMiddleClickTab(int)), SLOT(mouseMiddleClickTab(int)));
    // TabBar::tabRemove must be handled before tab_currentChanged
    connect( tabBar_, SIGNAL( currentChanged(int)), SLOT(tab_currentChanged(int)));
    connect( tabBar_, SIGNAL( contextMenu(QContextMenuEvent*,int)), SLOT( tab_contextMenu(QContextMenuEvent*,int)));
    connect( closeButton_, SIGNAL(clicked()), SIGNAL(closeButtonClicked()));
    connect(tabBar_, SIGNAL(tabMoved(int,int)),SLOT(widgetMoved(int,int)));
    connect(tabBar_, SIGNAL(tabCloseRequested(int)),SIGNAL(tabCloseRequested(int)));
}

void PsiTabWidget::setCloseIcon(const QIcon& icon) {
    closeButton_->setIcon(icon);
    closeButton_->setText("");
}

/**
 * Destructor
 */
PsiTabWidget::~PsiTabWidget() {
}

/**
 * Set the color of text on a tab.
 * \param tab Widget for the tab to change.
 * \param color Color to set text.
 */
void PsiTabWidget::setTabTextColor( QWidget* tab, const QColor& color) {
    for (int i = 0; i < count(); i++) {
        if (widget(i) == tab) {
            tabBar_->setTabTextColor(i, color);
        }
    }
}

/**
 * Returns the specified widget.
 * \param index Widget to return.
 * \return Specified widget.
 */
QWidget *PsiTabWidget::widget(int index) {
    return widgets_[index];
}

void PsiTabWidget::mouseDoubleClickTab(int tab) {
    emit mouseDoubleClickTab(widget(tab));
}

void PsiTabWidget::mouseMiddleClickTab(int tab) {
    emit mouseMiddleClickTab(widget(tab));
}

/**
 * Number of tabs/widgets
 */
int PsiTabWidget::count() {
    return tabBar_->count();
}

/**
 * Returns the widget of the current page
 */
QWidget *PsiTabWidget::currentPage() {
    if (currentPageIndex() == -1)
        return nullptr;
    return widgets_[currentPageIndex()];
}

void PsiTabWidget::tab_currentChanged(int tab) {
    // qt 4.4 sends -1 i case of an empty QTabbar, ignore that case.
    if (tab == -1) return;
    setCurrentPage(tab);
    emit currentChanged(currentPage());
}

/**
 * Returns the index of the current page
 */
int PsiTabWidget::currentPageIndex() {
    return tabBar_->currentIndex();
}

/**
 * Add the Widget to the tab stack.
 */
void PsiTabWidget::addTab(QWidget *widget, QString name, const QIcon &icon)
{
    Q_ASSERT(widget);
    if (widgets_.contains(widget)) {
        return;
    }
    widgets_.append(widget);
    stacked_->addWidget(widget);
    if (PsiOptions::instance()->getOption("options.ui.tabs.show-tab-icons").toBool())
        tabBar_->addTab(icon, name);
    else
        tabBar_->addTab(name);
    setLooks();
    showPage(currentPage());
    tabBar_->layoutTabs();
}

void PsiTabWidget::setLooks()
{
    const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
    if (!css.isEmpty()) {
        setStyleSheet(css);
    }
}

void PsiTabWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (tabBar_->multiRow()) {
        tabBar_->layoutTabs();
    }
}

/**
 * Selects the page for the specified widget.
 */
void PsiTabWidget::showPage(QWidget* widget) {
    for (int i = 0; i < count(); i++) {
        if (widgets_[i] == widget) {
            showPageDirectly(widget);
            tabBar_->setCurrentIndex(i);
        }
    }

}

/**
 * Selects the page for the specified widget (internal helper).
 */
void PsiTabWidget::showPageDirectly(QWidget* widget) {
    // FIXME move this back into showPage? should this be in the public interface?
    for (int i=0; i < count(); i++) {
        if (widgets_[i] == widget) {
            stacked_->setCurrentWidget(widget);
            // currentChanged is handled by tabBar_
            return;
        }
    }
}

void PsiTabWidget::setPagePinned(QWidget *page, bool pinned)
{
    foreach (QWidget *w, widgets_) {
        if (w == page) {
            tabBar_->setTabPinned(widgets_.indexOf(w), pinned);
            showPageDirectly(page);
            break;
        }
    }
}

bool PsiTabWidget::isPagePinned(QWidget *page)
{
    foreach (QWidget *w, widgets_) {
        if(w == page) {
            return tabBar_->isTabPinned(widgets_.indexOf(w));
        }
    }
    return false;
}

/**
 * Removes the page for the specified widget.
 */
void PsiTabWidget::removePage(QWidget* widget) {
    for (int i=0; i < count(); i++) {
        if (widgets_[i] == widget) {
            stacked_->removeWidget(widget);
            widgets_.remove(i);
            tabBar_->removeTab(i);
            // tabBar_ emits current changed if needed
        }
    }
}

/**
 * Finds the index of the widget (or -1 if missing).
 */
int PsiTabWidget::getIndex(QWidget* widget) {
    for (int i = 0; i < count(); i++) {
        if (widgets_[i] == widget) {
            return i;
        }
    }
    return -1;
}

/**
 * Set the text of the tab.
 */
void PsiTabWidget::setTabText(QWidget* widget, const QString& label) {
    int index = getIndex(widget);
    if (index == -1) {
        return;
    }
    tabBar_->setTabText(index, label);
}

/**
 * Set the icon of the tab.
 */
void PsiTabWidget::setTabIcon(QWidget *widget, const QIcon &icon)
{
    int index = getIndex(widget);
    if (index == -1 || !PsiOptions::instance()->getOption("options.ui.tabs.show-tab-icons").toBool()) {
        return;
    }
    tabBar_->setTabIcon(index, icon);
}

void PsiTabWidget::setCurrentPage(int index) {
    if (index >= 0 && index < count()) {
        showPage(widgets_[index]);
    }
}

void PsiTabWidget::removeCurrentPage() {
    removePage(currentPage());
}

void PsiTabWidget::setTabPosition(QTabWidget::TabPosition pos) {
    if (tabsPosition_ == pos) {
        return;
    }

    tabsPosition_ = pos;
    tabBar_->setShape(tabsPosition_ == QTabWidget::North ? QTabBar::RoundedNorth : QTabBar::RoundedSouth);

    layout_->removeItem(barLayout_);
    layout_->removeItem(stacked_);

    // addLayout sets parent and complains if it's already set
    barLayout_->setParent(nullptr);
    stacked_->setParent(nullptr);
    if (tabsPosition_ == QTabWidget::North) {
        layout_->addLayout(barLayout_);
        layout_->addLayout(stacked_);
    } else {
        layout_->addLayout(stacked_);
        layout_->addLayout(barLayout_);
    }
}

void PsiTabWidget::menu_aboutToShow() {
    clearMenu(menu_);
    bool vis = false;
    for (int i = 0; i < tabBar_->count(); i++) {
        QRect r = tabBar_->tabRect(i);
        bool newvis = tabBar_->rect().contains(r);
        if (newvis != vis) {
            menu_->addSeparator ();
            vis = newvis;
        }
        menu_->addAction(tabBar_->tabText(i))->setData(i+1);
    }
    emit aboutToShowMenu(menu_);
}

void PsiTabWidget::menu_triggered(QAction *act) {
    int idx = act->data().toInt();
    if (idx <= 0 || idx > tabBar_->count()) {
        // out of range
        // emit signal?
    } else {
        setCurrentPage(idx-1);
    }
}

void PsiTabWidget::tab_contextMenu( QContextMenuEvent * event, int tab) {
    emit tabContextMenu(tab, tabBar_->mapToGlobal(event->pos()), event);
}

QWidget* PsiTabWidget::page(int index) {
    Q_ASSERT(index >=0 && index < count());
    return widgets_[index];
}

/**
 * Show/hide the tab bar of this widget
 */
void PsiTabWidget::setTabBarShown(bool shown) {
    if (shown && tabBar_->isHidden()) {
        tabBar_->show();
    } else if (!shown && !tabBar_->isHidden()) {
        tabBar_->hide();
    }
}

/**
 * Show/hide the menu and close buttons that appear next to the tab bar
 */
void PsiTabWidget::setTabButtonsShown(bool shown) {
    if (shown && downButton_->isHidden()) {
        downButton_->show();
        closeButton_->show();
    } else if (!shown && !downButton_->isHidden()) {
        downButton_->hide();
        closeButton_->hide();
    }
}

/**
 * Enable/disable dragging of tabs
 */
void PsiTabWidget::setDragsEnabled(bool enabled) {
    static_cast<PsiTabBar *>(tabBar_)->setDragsEnabled(enabled);
}

void PsiTabWidget::setTabBarUpdateEnabled(bool b)
{
    tabBar_->setUpdateEnabled(b);
}

void PsiTabWidget::widgetMoved(int from, int to)
{
    if (from > to) {
        stacked_->removeWidget(widgets_[from]);
        widgets_.insert(to, 1, widgets_[from]);
        widgets_.remove(from+1);
        stacked_->insertWidget(to,widgets_[to]);
    }
    else {
        stacked_->removeWidget(widgets_[from]);
        widgets_.insert(to+1, 1, widgets_[from]);
        widgets_.remove(from,1);
        stacked_->insertWidget(to,widgets_[to]);
    }

    emit currentChanged(currentPage());

};
