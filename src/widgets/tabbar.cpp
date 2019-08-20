/*
 * tabbar.cpp
 * Copyright (C) 2013-2016  Ivan Romanov <drizt@land.ru>
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

#include "tabbar.h"
#include "iconset.h"

#include <QAbstractButton>
#include <QPixmap>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QMouseEvent>
#include <QApplication>
#include <QLine>
#include <QDrag>
#include <QMimeData>
#include <memory>
//#include <QDebug>

#define PINNED_CHARS 6

// Do not count invisible &
#define PINNED_TEXT(text) text.left(text.left(PINNED_CHARS).contains("&") ? (PINNED_CHARS + 1) : PINNED_CHARS)

class CloseButton : public QAbstractButton
{
    Q_OBJECT

public:
    CloseButton(QWidget *parent = nullptr);

    QSize sizeHint() const;
    inline QSize minimumSizeHint() const
        { return sizeHint(); }
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
};

struct RowSf
{
    RowSf() : number(0), sf(0.) {}

    int number;
    double sf;
};

typedef QList<RowSf> LayoutSf;

class TabBar::Private
{
public:
    Private(TabBar *base);

    void layoutTabs();
    int pinnedTabWidthHint() const;
    QSize tabSizeHint(QStyleOptionTab tab) const;
    void balanseCloseButtons();
    bool indexAtBottom(int index) const;

    TabBar *q;
    QList<QStyleOptionTab> hackedTabs;
    QList<CloseButton*> closeButtons;
    bool tabsClosable;
    bool multiRow;
    int hoverTab;
    bool dragsEnabled;
    int dragTab;
    int dragInsertIndex;
    int dragHoverTab;
    QPoint mousePressPoint;
    int pinnedTabs;
    bool update;
    bool stopRecursive;
    bool indexAlwaysAtBottom;

    struct {
        QList<int> tabs;
        int barWidth;
        int rows;
        double baseSf;
        LayoutSf layout;
    } cachedLayout;
};

TabBar::Private::Private(TabBar *base)
    : q(base)
    , hackedTabs()
    , closeButtons()
    , tabsClosable(false)
    , multiRow(false)
    , hoverTab(-1)
    , dragsEnabled(true)
    , dragTab(-1)
    , dragInsertIndex(-1)
    , dragHoverTab(-1)
    , mousePressPoint()
    , pinnedTabs(0)
    , update(true)
    , stopRecursive(false)
    , indexAlwaysAtBottom(false)
    , cachedLayout({ QList<int>(), 0, 0, 0., LayoutSf() })
{
    balanseCloseButtons();
}

LayoutSf possibleLayouts2(const QList<int> &tabs, int barWidth, int rows, double baseSf)
{
    std::unique_ptr<int[]> layouts(new int[50 * 10000]);
    for (int i = 0; i < rows; ++i) {
        layouts[size_t(i)] = i;
    }

    int *pPrevLayout = &layouts[0];
    int *pLayout = &layouts[size_t(rows)];
    int nTabs = tabs.size();


    // Fill layouts
    while (true) {
        for (int i = 0; i < rows; ++i) {
            pLayout[i] = pPrevLayout[i];
        }

        int i = rows - 1;
        while (i > 0) {
            int base = pLayout[i] + 1;
            if (base <= nTabs - rows + i) {
                for (int j = i; j < rows; j++)
                    pLayout[j] = base++;

                break;
            }
            else {
                i--;
            }
        }

        if (i == 0)
            break;

        pPrevLayout = pLayout;
        pLayout += rows;
    }

    int *pGoodLayout = nullptr;
    double minDSf = 10000.; // Just huge number

    for (pPrevLayout = &layouts[0]; pPrevLayout < pLayout; pPrevLayout += rows) {
        bool addRow = true;
        double sf = 0.;
        for (int i = 0; i < rows; ++i) {
            int tabsWidth = 0;
            int end = i == rows - 1 ? nTabs : pPrevLayout[i + 1];
            for (int j = pPrevLayout[i]; j < end; ++j)
                tabsWidth += tabs[j];

            if (tabsWidth > barWidth && end - pPrevLayout[i] > 1) {
                addRow = false;
                break;
            }

            double curSf = static_cast<double>(barWidth) / tabsWidth;
            sf += qAbs(baseSf - curSf);
            if (i == rows - 1) {
                if (sf < minDSf) {
                    pGoodLayout = pPrevLayout;
                    minDSf = sf;
                }
                else {
                    addRow = false;
                    break;
                }
            }
        }

        if (!addRow)
            continue;
    }

    LayoutSf res;
    if (pGoodLayout) {
        for (int i = 0; i < rows; ++i) {
            RowSf rowSf;
            rowSf.number = pGoodLayout[i];

            int tabsWidth = 0;
            int end = i == rows - 1 ? nTabs : pGoodLayout[i + 1];
            for (int j = pGoodLayout[i]; j < end; ++j)
                tabsWidth += tabs[j];

            rowSf.sf = static_cast<double>(barWidth) / tabsWidth;

            res << rowSf;
        }
    }

    return res;
}

LayoutSf possibleLayouts(const QList<int> &tabs, int barWidth, int rows, double baseSf)
{
    // Some safe combinations
    if (                 tabs.size() <= 18
        || (rows <= 6 && tabs.size() <= 22)
        || (rows <= 5 && tabs.size() <= 28)
        || (rows <= 4 && tabs.size() <= 40)) {

        return possibleLayouts2(tabs, barWidth, rows, baseSf);
    }

    // Will believe that 3 rows is enough good number to avoid overflow of layouts array
    // in possibleLayouts2 function.
    // Also look at Combination Formula to understand how it works.
    LayoutSf layoutSf;
    int i = 0;
    int step = 3;
    int extratab = (tabs.size() % rows) ? 1 : 0;
    while (i < rows) {
        if (rows - i - step == 1)
            step = 2;

        int rows2 = qMin(step, rows - i);
        int startPos = i * (tabs.size() / rows + extratab);
        int length = step * (tabs.size() / rows + extratab);
        QList<int> tabs2 = tabs.mid(startPos, length);

        LayoutSf newLayoutSf = possibleLayouts2(tabs2, barWidth, rows2, baseSf);
        if (newLayoutSf.isEmpty()) {
            layoutSf.clear();
            break;
        }
        for (int j = 0; j < newLayoutSf.size(); ++j) {
            newLayoutSf[j].number += startPos;
        }
        layoutSf += newLayoutSf;
        i += step;
    }

    return layoutSf;
}

void TabBar::Private::layoutTabs()
{
    if (!update)
        return;

    pinnedTabs = qMin(pinnedTabs, q->count());
    hackedTabs.clear();

    QTabBar::ButtonPosition closeSide = static_cast<QTabBar::ButtonPosition>(q->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, q));
    // Tabs maybe 0 width in all-in-one mode
    int barWidth = qMax(q->width(), 1);
    int tabsWidthHint = 0;

    int pinnedTabWidth = pinnedTabWidthHint();
    int pinnedInRow = barWidth / pinnedTabWidth;

    // Protect zero divide
    if (!pinnedInRow)
        pinnedInRow = 1;

    int pinnedRows = pinnedTabs / pinnedInRow;
    double pinnedSf = static_cast<double>(barWidth) / (pinnedInRow * pinnedTabWidth);

    // Prepare hacked tabs
    for (int i = 0; i < q->count(); i++) {
        QStyleOptionTab tab;
        q->initStyleOption(&tab, i);
        if (i == 0) {
            tab.rect.setLeft(0);
        }

        tab.state &= ~QStyle::State_MouseOver;
        tab.position = QStyleOptionTab::Beginning;

        if (tabsClosable && i >= pinnedTabs) {
            tab.rect.setWidth(tab.rect.width() + closeButtons.at(i)->size().width());
            if (closeSide == QTabBar::LeftSide) {
                tab.leftButtonSize = closeButtons.at(i)->size();
            }
            else {
                tab.rightButtonSize = closeButtons.at(i)->size();
            }
        }

        tab.rect.setSize(tabSizeHint(tab));
        // Make pinned tab if need
        if (i < pinnedTabs){
            tab.text = PINNED_TEXT(tab.text);
            tab.rect.setWidth(pinnedTabWidth);
        }
        hackedTabs << tab;
    }

    // Extra checking for no any tabs (maybe can be dropped?)
    if (hackedTabs.isEmpty())
        return;

    int firstNormalTab = pinnedInRow * pinnedRows;
    // Not enough space for normal tab in row with pinned tabs
    if (pinnedTabs == hackedTabs.size()
        || (pinnedTabs
            && (barWidth - pinnedTabWidth * (pinnedTabs - pinnedRows * pinnedInRow) < hackedTabs.at(pinnedTabs).rect.width()))) {

        pinnedRows++;
        firstNormalTab = pinnedTabs;
    }

    // Calculate all normal tabs (with pinned tale) width hint
    for (int i = firstNormalTab; i < hackedTabs.size(); ++i) {
        tabsWidthHint += hackedTabs.at(i).rect.width();
    }

    int normalRows = pinnedTabs < hackedTabs.size() ? tabsWidthHint / barWidth + 1 : 0;
    int rows = normalRows + pinnedRows;

    if (rows > hackedTabs.size()) {
        rows = hackedTabs.size();
        normalRows = rows - pinnedRows;
    }

    double sf = static_cast<double>(barWidth * normalRows) / tabsWidthHint;
    LayoutSf layout;
    if (rows == 1 || hackedTabs.size() == 1) {
        // Only one row in bar
        rows = 1;
        normalRows = 1;
        layout << RowSf();
        layout[0].number = 0;
        layout[0].sf = qMin(1.5, sf);
    }
    else {
        QList<int> tabWidths;
        for (int i = firstNormalTab; i < hackedTabs.size(); ++i) {
            tabWidths << hackedTabs.at(i).rect.width();
        }

        if (normalRows) {
            while (layout.isEmpty()) {
                // Speed optimization
                if (!cachedLayout.layout.isEmpty()
                    && cachedLayout.tabs == tabWidths
                    && cachedLayout.barWidth == barWidth
                    && cachedLayout.baseSf == sf
                    && cachedLayout.rows == normalRows) {

                    layout = cachedLayout.layout;
                    break;
                }
                else {
                    layout = possibleLayouts(tabWidths, barWidth, normalRows, sf);
                }
                if (layout.isEmpty()) {
                    normalRows++;
                    rows++;
                    sf = static_cast<double>(barWidth * normalRows) / tabsWidthHint;
                }
            }
            cachedLayout.layout = layout;
            cachedLayout.barWidth = barWidth;
            cachedLayout.baseSf = sf;
            cachedLayout.tabs = tabWidths;
            cachedLayout.rows = normalRows;
        }

        // Add pinned tabs to layout
        for (int i = 0; i < layout.size(); ++i) {
            layout[i].number += firstNormalTab;
        }

        for (int i = pinnedRows - 1; i >= 0; --i) {
            RowSf rowSf;
            rowSf.number = i * pinnedInRow;
            rowSf.sf = pinnedSf;
            layout.prepend(rowSf);
        }
    }

    // Calculate size and position for all tabs
    for (int i = 0; i < rows; ++i) {
        RowSf &row = layout[i];
        int endTab = i == rows - 1 ? hackedTabs.size() : layout[i + 1].number;
        int currentRowWidth = 0;
        int bottom;
        if (q->shape() == QTabBar::RoundedNorth)
            bottom = i * (hackedTabs[0].rect.height() - 2);
        else
            bottom = (rows - i - 1) * (hackedTabs[0].rect.height() - 2);

        for (int j = row.number; j < endTab; j++) {
            QStyleOptionTab &tab = hackedTabs[j];
            int tabWidth = tab.rect.width();
            if (rows > 1 && (j < firstNormalTab || j >= pinnedTabs)) {
                tabWidth *= int(row.sf);
                tab.rect.setWidth(tabWidth);
            }
            tab.rect.moveTop(bottom);
            tab.rect.moveLeft(currentRowWidth);

            if (currentRowWidth == 0) {
                if (endTab == j + 1) {
                    tab.position = QStyleOptionTab::OnlyOneTab;
                    if (rows > 1)
                        tab.rect.setRight(barWidth - 1);
                }
                else {
                    currentRowWidth += tabWidth;
                    tab.position = QStyleOptionTab::Beginning;
                }
            }
            else {
                if (j < endTab - 1) {
                    currentRowWidth += tabWidth;
                    tab.position = QStyleOptionTab::Middle;
                }
                else {
                    tab.position = QStyleOptionTab::End;

                    if (rows > 1)
                        tab.rect.setRight(barWidth - 1);
                    currentRowWidth = 0;
                }
            }
        }
    }

#ifdef Q_OS_MAC
    if (rows == 1) {
        int offset = (barWidth - hackedTabs.last().rect.right()) / 2;
        for (int i = 0; i < hackedTabs.size(); ++i)
            hackedTabs[i].rect.adjust(offset, 0, offset, 0);
    }
#endif

    q->setMinimumSize(0, q->sizeHint().height());
    q->resize(q->sizeHint());
}

inline static bool verticalTabs(QTabBar::Shape shape)
{
    return shape == QTabBar::RoundedWest
           || shape == QTabBar::RoundedEast
           || shape == QTabBar::TriangularWest
           || shape == QTabBar::TriangularEast;
}

int TabBar::Private::pinnedTabWidthHint() const
{
    QStyleOptionTab opt;
    q->initStyleOption(&opt, 0);
    opt.leftButtonSize = QSize();
    opt.rightButtonSize = QSize();
    opt.text = "XXX";
    opt.text = QString(PINNED_CHARS, 'X');
    QSize iconSize = opt.iconSize;
    int hframe = q->style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, q);
    int vframe = q->style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, q);
    QFont f = q->font();
    f.setBold(true);

    const QFontMetrics fm(f);

    int maxWidgetHeight = qMax(opt.leftButtonSize.height(), opt.rightButtonSize.height());
    int maxWidgetWidth = qMax(opt.leftButtonSize.width(), opt.rightButtonSize.width());

    int padding = 0;
    if (!opt.icon.isNull())
        padding += 4;

    QSize csz;
    if (verticalTabs(q->shape())) {
        csz = QSize( qMax(maxWidgetWidth, qMax(fm.height(), iconSize.height())) + vframe,
                     fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe + padding);
    } else {
        csz = QSize(fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe + padding,
                    qMax(maxWidgetHeight, qMax(fm.height(), iconSize.height())) + vframe);
    }

    QSize retSize = q->style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, csz, q);
    return retSize.width() + 5;

}

QSize TabBar::Private::tabSizeHint(QStyleOptionTab opt) const
{
    QSize iconSize = opt.iconSize;
    int hframe = q->style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, q);
    int vframe = q->style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, q);
    QFont f = q->font();
    f.setBold(true);

    const QFontMetrics fm(f);

    int maxWidgetHeight = qMax(opt.leftButtonSize.height(), opt.rightButtonSize.height());
    int maxWidgetWidth = qMax(opt.leftButtonSize.width(), opt.rightButtonSize.width());

    int widgetWidth = 0;
    int widgetHeight = 0;
    int padding = 0;
    if (!opt.leftButtonSize.isEmpty()) {
        padding += 4;
        widgetWidth += opt.leftButtonSize.width();
        widgetHeight += opt.leftButtonSize.height();
    }
    if (!opt.rightButtonSize.isEmpty()) {
        padding += 4;
        widgetWidth += opt.rightButtonSize.width();
        widgetHeight += opt.rightButtonSize.height();
    }
    if (!opt.icon.isNull())
        padding += 4;

    QSize csz;
    if (verticalTabs(q->shape())) {
        csz = QSize( qMax(maxWidgetWidth, qMax(fm.height(), iconSize.height())) + vframe,
                     fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe + widgetHeight + padding);
    } else {
        csz = QSize(fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe
                    + widgetWidth + padding,
                    qMax(maxWidgetHeight, qMax(fm.height(), iconSize.height())) + vframe);
    }

    QSize retSize = q->style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, csz, q);
    return retSize;

}

void TabBar::Private::balanseCloseButtons()
{
    pinnedTabs = qMin(pinnedTabs, q->count());

    if (tabsClosable && multiRow) {
        while (closeButtons.size() < q->count()) {
            CloseButton *cb = new CloseButton(q);
            closeButtons << cb;
            cb->show();
            connect(cb, SIGNAL(clicked()), q, SLOT(closeTab()));
        }

        while (closeButtons.size() > q->count()) {
            closeButtons.takeLast()->deleteLater();
        }

        for (int i = 0; i < pinnedTabs && i < closeButtons.size(); ++i) {
            closeButtons.at(i)->hide();
        }
        for (int i = pinnedTabs; i < closeButtons.size(); ++i) {
            closeButtons.at(i)->show();
        }
    }
    else {
        qDeleteAll(closeButtons);
        closeButtons.clear();
    }
}

bool TabBar::Private::indexAtBottom(int index) const
{
    if (!indexAlwaysAtBottom)
        return true;

    int lastBeginTab = hackedTabs.size() - 1;
    while (lastBeginTab > 0
           && hackedTabs.at(lastBeginTab).position != QStyleOptionTab::Beginning
           && hackedTabs.at(lastBeginTab).position != QStyleOptionTab::OnlyOneTab) {

        lastBeginTab--;
    }

    return index >= lastBeginTab || index < pinnedTabs;
}

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    d = new Private(this);
#ifdef Q_OS_LINUX
    // Workaround for KDE5 breeze problem with wrong window dragging on TabBar clicking
    qApp->installEventFilter(this);
#else
    installEventFilter(this);
#endif
}

TabBar::~TabBar()
{
    delete d;
}

void TabBar::layoutTabs()
{
    setMouseTracking(true);
    if (d->multiRow) {
        d->layoutTabs();
        if (!d->indexAtBottom(currentIndex())) {
            setCurrentIndex(currentIndex());
        }
    }
    update();
}

void TabBar::setMultiRow(bool b)
{
    if (b == d->multiRow) {
        return;
    }

    d->multiRow = b;
    setMouseTracking(b);
    setAcceptDrops(b);

    if (b) {
        d->tabsClosable = QTabBar::tabsClosable();
        setElideMode(Qt::ElideNone);
        QTabBar::setTabsClosable(false);
    }
    else {
        QTabBar::setTabsClosable(d->tabsClosable);
        d->tabsClosable = false;
    }

    d->balanseCloseButtons();
    if (b) {
        // setUsesScrollButtons(false);
        layoutTabs();
    }
    else {
        d->hackedTabs.clear();
        update();
    }
}

void TabBar::setCurrentIndex(int index)
{
    if (!d->multiRow || d->hackedTabs.isEmpty()) {
        QTabBar::setCurrentIndex(index);
        return;
    }

    if (index == currentIndex() && d->indexAtBottom(index))
        return;

    if (d->stopRecursive)
        return;

    d->stopRecursive = true;
    d->pinnedTabs = qMin(d->pinnedTabs, count());

    if (!d->indexAtBottom(index)) {
        int curLastBeginTab = index;
        while (d->hackedTabs.at(curLastBeginTab).position != QStyleOptionTab::Beginning
               && d->hackedTabs.at(curLastBeginTab).position != QStyleOptionTab::OnlyOneTab && curLastBeginTab > d->pinnedTabs) {

            curLastBeginTab--;
        }

        if (d->hackedTabs.at(curLastBeginTab).position == QStyleOptionTab::OnlyOneTab) {

            moveTab(curLastBeginTab, count() - 1);
            index = count() - 1;
        }
        else {
            int curLastEndingTab = curLastBeginTab;
            while (d->hackedTabs.at(curLastEndingTab).position != QStyleOptionTab::End) {
                curLastEndingTab++;
            }

            // Try to move whole line
            for (int i = curLastBeginTab; i <= curLastEndingTab; i++) {
                moveTab(curLastBeginTab, count() - 1);
            }
            index = count() - (curLastEndingTab - curLastBeginTab) - 1 + (index - curLastBeginTab);
        }
    }

    QTabBar::setCurrentIndex(index);
    d->layoutTabs();

    // Extra checking for current tab at bottom
    if (!d->indexAtBottom(index)) {
        moveTab(index, count() - 1);
        index = count() - 1;
        QTabBar::setCurrentIndex(index);
        d->layoutTabs();
    }

    d->stopRecursive = false;
}

void TabBar::setTabText(int index, const QString & text)
{
    QTabBar::setTabText(index, text);
    layoutTabs();
}

void TabBar::setTabTextColor(int index, const QColor & color)
{
    QTabBar::setTabTextColor(index, color);
    layoutTabs();
}

void TabBar::setTabIcon(int index, const QIcon &icon)
{
    QTabBar::setTabIcon(index, icon);
    layoutTabs();
}

QRect TabBar::tabRect(int index) const
{
    if (d->multiRow) {
        if (index < d->hackedTabs.size() && index >= 0)
            return d->hackedTabs[index].rect;
        else
            return QRect();
    }
    else {
        return QTabBar::tabRect(index);
    }
}

QWidget *TabBar::tabButton(int index, ButtonPosition position) const
{
    ButtonPosition closeButtonPos = static_cast<ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
    if (!d->multiRow || position != closeButtonPos)
        return QTabBar::tabButton(index, position);

    Q_ASSERT(index < d->closeButtons.size());
    return index < d->closeButtons.size() ? d->closeButtons[index] : nullptr;
}

int TabBar::tabAt(const QPoint &position) const
{
    if (d->multiRow) {
        int tab = -1;
        for (int i = 0; i < d->hackedTabs.count(); ++i) {
            if (d->hackedTabs[i].rect.contains(position)) {
                tab = i;
                break;
            }
        }
        return tab;
    }
    else {
        return QTabBar::tabAt(position);
    }
}

bool TabBar::eventFilter(QObject *watched, QEvent *event)
{
    if (d->multiRow && watched == this && event->type() == QEvent::MouseButtonPress) {
        mousePressEvent(static_cast<QMouseEvent*>(event));
        event->accept();
        return true;
    }
    return QTabBar::eventFilter(watched, event);
}

void TabBar::setUpdateEnabled(bool b)
{
    d->update = b;
    if (b) {
        layoutTabs();
    }
    else {
        d->hackedTabs.clear();
    }
}

bool TabBar::multiRow() const
{
    return d->multiRow;
}

/*
 * Enable/disable dragging of tabs
 */
void TabBar::setDragsEnabled(bool enabled)
{
    d->dragsEnabled = enabled;
}

void TabBar::setTabPinned(int index, bool pinned)
{
    int newPos = d->pinnedTabs + (pinned ? 0 : -1);
    int tempPos = (index+1) < d->hackedTabs.count() ? index + 1 : index - 1; //get nearby tab index
    if (index != newPos) {
        if(index == TabBar::currentIndex()) { //Check is this tab active
            setCurrentIndex(tempPos); // Dirty hack. Set nearby tab as active
        }
        moveTab(index, newPos);
    }

    d->pinnedTabs += pinned ? +1 : -1;

    d->layoutTabs();
    setCurrentIndex(newPos);
    d->balanseCloseButtons();
    update();
}

bool TabBar::isTabPinned(int index)
{
    return index < d->pinnedTabs;
}

void TabBar::setCurrentIndexAlwaysAtBottom(bool b)
{
    if (b == d->indexAlwaysAtBottom)
        return;

    d->indexAlwaysAtBottom = b;
    layoutTabs();
}

bool TabBar::currentIndexAlwaysAtBottom() const
{
    return d->indexAlwaysAtBottom;
}

QSize TabBar::minimumSizeHint() const
{
    return QSize(0, sizeHint().height());
}

QSize TabBar::sizeHint() const
{
    // use own sizeHint only for single row mode
    if (!d->multiRow) {
        return QTabBar::sizeHint();
    }

    QList<QStyleOptionTab> tabs = d->hackedTabs;

    QRect rect;
    for (int i=0; i < tabs.size(); i++) {
        rect = rect.united(tabs.at(i).rect);
    }

    QSize size = rect.size();
    size.setWidth(width());
    return size;
}

QSize TabBar::tabSizeHint(int index) const
{
    // use own sizeHint only for single row mode
    if (!d->multiRow) {
        return QTabBar::tabSizeHint(index);
    }

    if (index < 0 || d->hackedTabs.size() <= index) {
        return QSize();
    }

    QStyleOptionTab opt = d->hackedTabs.at(index);
    return d->tabSizeHint(opt);
}

void TabBar::setTabsClosable(bool b)
{
    if (!d->multiRow) {
        QTabBar::setTabsClosable(b);
        return;
    }

    if (d->tabsClosable == b) {
        return;
    }

    d->tabsClosable = b;
    d->balanseCloseButtons();
    d->layoutTabs();
}

bool TabBar::tabsClosable() const
{
    return d->tabsClosable;
}

// stealed from qtabbar_p.h
static void initStyleBaseOption(QStyleOptionTabBarBase *optTabBase, QTabBar *tabbar, QSize size)
{
    QStyleOptionTab tabOverlap;
    tabOverlap.shape = tabbar->shape();
    int overlap = tabbar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, tabbar);
    QWidget *theParent = tabbar->parentWidget();
    optTabBase->init(tabbar);
    optTabBase->shape = tabbar->shape();
    optTabBase->documentMode = tabbar->documentMode();
    if (theParent && overlap > 0) {
        QRect rect;
        switch (tabOverlap.shape) {
        case QTabBar::RoundedNorth:
        case QTabBar::TriangularNorth:
            rect.setRect(0, size.height()-overlap, size.width(), overlap);
            break;
        case QTabBar::RoundedSouth:
        case QTabBar::TriangularSouth:
            rect.setRect(0, 0, size.width(), overlap);
            break;
        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            rect.setRect(0, 0, overlap, size.height());
            break;
        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
            rect.setRect(size.width() - overlap, 0, overlap, size.height());
            break;
        }
        optTabBase->rect = rect;
    }
}

void TabBar::paintEvent(QPaintEvent *event)
{
    // use own painting only for multi row mode
    if (!d->multiRow) {
        QTabBar::paintEvent(event);
        return;
    }

    QStylePainter p(this);
    QList<QStyleOptionTab> tabs = d->hackedTabs;

    for (int i = 0; i < tabs.size() && i < d->pinnedTabs; ++i) {
        tabs[i].leftButtonSize = QSize();
        tabs[i].rightButtonSize = QSize();
    }

    int selected = currentIndex();
    if (drawBase()) {
        QStyleOptionTabBarBase optTabBase;
        initStyleBaseOption(&optTabBase, this, size());

        if (selected >= 0) {
            optTabBase.selectedTabRect = tabs[selected].rect;
        }

        for (int i = 0; i < tabs.size(); ++i)
            optTabBase.tabBarRect |= tabs[i].rect;

        p.drawPrimitive(QStyle::PE_FrameTabBarBase, optTabBase);
    }

    int rowHeight = 0;
    if (selected >= 0) {
        rowHeight = tabs[selected].rect.height();
    } else if (tabs.count()) {
        rowHeight = tabs[0].rect.height();
    }

    // There is some problems when tabs are painted not in first row.
    // Draw on a pixmap like a painting in the first row. Then move image
    // to real TabBar widget.
    QPixmap pixmap(width(), rowHeight);
    pixmap.fill(Qt::transparent);
    QStylePainter pp(&pixmap, this);
    bool drawSelected = false;
    QPixmap pinPixmap = IconsetFactory::iconPixmap("psi/pin");
    for (int i = 0; i < tabs.size(); i++) {
        QStyleOptionTab tab = tabs[i];
        if (i != selected) {
            if (i == d->hoverTab)
                tab.state |= QStyle::State_MouseOver;

            if (shape() == QTabBar::RoundedNorth)
                tab.rect.moveBottom(rowHeight - 1);
            else
                tab.rect.moveTop(0);

            // Just dd.drawControl works incorrect with KDE5 breeze style
            pp.style()->drawControl(QStyle::CE_TabBarTab, &tab, &pp);
            if (i < d->pinnedTabs) {
                pp.drawPixmap(tab.rect.topRight() - QPoint(pinPixmap.width(), -3), pinPixmap);
            }
        }
        else {
            drawSelected = true;
        }


        if (tab.position == QStyleOptionTab::End || tab.position == QStyleOptionTab::OnlyOneTab) {
            if (drawSelected) {
                // Draw current tab in the last order
                tab = tabs.at(selected);
                if (shape() == QTabBar::RoundedNorth)
                    tab.rect.moveBottom(rowHeight - 1);
                else
                    tab.rect.moveTop(0);

                // Draw tab shape
                // Use red color as tab frame
                QPalette oldPalette = tab.palette;
                tab.palette.setColor(QPalette::Foreground, Qt::red);
                tab.palette.setColor(QPalette::Light, Qt::red);
                tab.palette.setColor(QPalette::Dark, Qt::red);
                pp.drawControl(QStyle::CE_TabBarTabShape, tab);
                tab.palette = oldPalette;

                // Use bold font for current tab
                QFont f = pp.font();
                f.setBold(true);
                pp.save();
                pp.setFont(f);
                pp.drawControl(QStyle::CE_TabBarTabLabel, tab);
                pp.restore();

                if (selected < d->pinnedTabs) {
                    pp.drawPixmap(tab.rect.topRight() - QPoint(pinPixmap.width(), -3), pinPixmap);
                }

                drawSelected = false;
            }

            QRect rect(0, tabs.at(i).rect.top(), width(), rowHeight);
            p.drawItemPixmap(rect, Qt::AlignCenter, pixmap);
            pixmap.fill(Qt::transparent);
        }
    }

    ButtonPosition closeSide = static_cast<QTabBar::ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
    QStyle::SubElement se = (closeSide == LeftSide ? QStyle::SE_TabBarTabLeftButton : QStyle::SE_TabBarTabRightButton);
    if (d->tabsClosable) {
        for (int i = 0; i < tabs.size(); i++) {
            QStyleOptionTab opt;
            opt = tabs.at(i);

            QRect rect = style()->subElementRect(se, &opt, this);
            rect.setTop(rect.top() + opt.rect.top());
            QPoint p = rect.topLeft();
            d->closeButtons.at(i)->move(p);
        }
    }

    // Draw tab inserting position
    if (d->dragInsertIndex > -1) {
        QLine line;
        if (d->dragInsertIndex == d->dragHoverTab) {
            line.setP1(tabs[d->dragHoverTab].rect.topLeft());
            line.setP2(tabs[d->dragHoverTab].rect.bottomLeft());
        }
        else {
            line.setP1(tabs[d->dragHoverTab].rect.topRight());
            line.setP2(tabs[d->dragHoverTab].rect.bottomRight());
        }
        p.save();
        p.setPen(Qt::red);
        p.drawLine(line);
        p.restore();
    }
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    if (!d->multiRow) {
        QTabBar::mousePressEvent(event);
    }
    else {
        d->mousePressPoint = event->pos();
        event->accept();
    }
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (!d->multiRow) {
        QTabBar::mouseReleaseEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    int curTab = -1;
    for (int i = 0; i < count(); i++) {
        if (d->hackedTabs.at(i).rect.contains(event->pos())) {
            curTab = i;
            break;
        }
    }

    if (curTab < 0)
        return;


    setCurrentIndex(curTab);
    d->layoutTabs();
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (d->multiRow && event->buttons() == Qt::NoButton) {
        int newHoverTab = tabAt(event->pos());
        if (newHoverTab != d->hoverTab) {
            d->hoverTab = newHoverTab;
            update();
        }
    }

    if (!d->dragsEnabled)
        return;

    if (!(event->buttons() & Qt::LeftButton))
        return;

    if (!d->multiRow) {
        QTabBar::mouseMoveEvent(event);
    }
    else {
        if (d->dragTab == -1) {
            if ((event->pos() - d->mousePressPoint).manhattanLength() >= QApplication::startDragDistance()) {
                // Do not allow to drag single pinned tab
                int tab = tabAt(d->mousePressPoint);
                if (tab != 0 || d->pinnedTabs != 1)
                    d->dragTab = tab;
                update();
            }
        }
        if (d->dragTab > -1) {
            QStyleOptionTab tab = d->hackedTabs[d->dragTab];
            QPixmap pixmap(tab.rect.size());
            tab.rect.moveTo(0, 0);
            tab.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_Selected;
            pixmap.fill(Qt::transparent);
            QStylePainter pp(&pixmap, this);
            QFont f = pp.font();
            f.setBold(true);
            pp.setFont(f);
            pp.style()->drawControl(QStyle::CE_TabBarTab, &tab, &pp);

            QDrag *drag = new QDrag(this);
            drag->setMimeData(new QMimeData);
            drag->setPixmap(pixmap);
            QPoint hotSpot = event->pos() - d->hackedTabs[d->dragTab].rect.topLeft();
            if (hotSpot.x() < 0)
                hotSpot.setX(0);
            else if (hotSpot.x() > tab.rect.width())
                hotSpot.setX(tab.rect.width());

            if (hotSpot.y() < 0)
                hotSpot.setY(0);
            else if (hotSpot.y() > tab.rect.height())
                hotSpot.setY(tab.rect.height());

            drag->setHotSpot(hotSpot);
            drag->exec();

            d->hoverTab = -1;
        }
    }
}

void TabBar::dragMoveEvent(QDragMoveEvent *event)
{
    int newDragHoverTab = tabAt(event->pos());
    int newDragInsertIndex = newDragHoverTab;

    // Try to guess that need to insert in the end
    if (newDragHoverTab == -1) {
        QPoint p = d->hackedTabs.last().rect.topRight();
        if (event->pos().x() > p.x() && event->pos().y() > p.y()) {
            newDragHoverTab = d->hackedTabs.size() - 1;
            newDragInsertIndex = newDragHoverTab;
        }
    }

    if (newDragInsertIndex > -1) {
        int x = event->pos().x() - d->hackedTabs[newDragInsertIndex].rect.left();
        if (x * 2 > d->hackedTabs[newDragInsertIndex].rect.width())
            newDragInsertIndex++;
    }

    if ((newDragInsertIndex != d->dragInsertIndex || newDragHoverTab != d->dragHoverTab)
        && ((d->dragTab >= d->pinnedTabs && newDragInsertIndex >= d->pinnedTabs)
            || (d->dragTab < d->pinnedTabs && newDragInsertIndex <= d->pinnedTabs))) {
        d->dragInsertIndex = newDragInsertIndex;
        d->dragHoverTab = newDragHoverTab;
        update();
    }
}

void TabBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->source() == this) {
        event->accept();
    } else {
        event->ignore();
    }
}

void TabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);

    d->dragHoverTab = -1;
    d->dragInsertIndex = -1;
    update();
}

void TabBar::dropEvent(QDropEvent *event)
{
    if (event->source() == this) {
        if (d->dragInsertIndex > -1 && d->dragInsertIndex != d->dragTab && d->dragInsertIndex != d->dragTab + 1) {
            int curTab = d->dragInsertIndex > d->dragTab ? d->dragInsertIndex - 1 : d->dragInsertIndex;
            moveTab(d->dragTab, curTab);
            d->layoutTabs();
            setCurrentIndex(curTab);
        }
        d->dragTab = -1;
        d->dragInsertIndex = -1;
        update();
    }
}

void TabBar::leaveEvent(QEvent *event)
{
    if (d->hoverTab != -1) {
        d->hoverTab = -1;
        update();
    }
    QTabBar::leaveEvent(event);
}

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);

    if (!d->multiRow) {
        return;
    }

    d->balanseCloseButtons();
    d->layoutTabs();

}

void TabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);

    if (!d->multiRow) {
        return;
    }

    if (index < d->pinnedTabs)
        d->pinnedTabs--;

    d->balanseCloseButtons();
    d->layoutTabs();
}

void TabBar::closeTab()
{
    CloseButton *cb = qobject_cast<CloseButton*>(sender());
    int index = d->closeButtons.indexOf(cb);
    emit tabCloseRequested(index);
}

CloseButton::CloseButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setToolTip(tr("Close Tab"));
    resize(sizeHint());
}

QSize CloseButton::sizeHint() const
{
    ensurePolished();
    int width = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, nullptr, this);
    int height = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, nullptr, this);
    return QSize(width, height);
}

void CloseButton::enterEvent(QEvent *event)
{
    if (isEnabled())
        update();
    QAbstractButton::enterEvent(event);
}

void CloseButton::leaveEvent(QEvent *event)
{
    if (isEnabled())
        update();
    QAbstractButton::leaveEvent(event);
}

void CloseButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt;
    opt.init(this);
    opt.state |= QStyle::State_AutoRaise;
    if (isEnabled() && underMouse() && !isChecked() && !isDown())
        opt.state |= QStyle::State_Raised;
    if (isChecked())
        opt.state |= QStyle::State_On;
    if (isDown())
        opt.state |= QStyle::State_Sunken;

    if (const TabBar *tb = qobject_cast<const TabBar*>(parent())) {
        int index = tb->currentIndex();
        if (index >= 0) {
            QTabBar::ButtonPosition position = static_cast<QTabBar::ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, tb));
            if (tb->tabButton(index, position) == this)
                opt.state |= QStyle::State_Selected;
        }
    }

    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}

#include "tabbar.moc"
