/*
 * iconselect.cpp - class that allows user to select an PsiIcon from an Iconset
 * Copyright (C) 2003-2005  Michail Pishchagin
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

#include "iconselect.h"

#include "actionlineedit.h"
#include "emojiregistry.h"
#include "iconaction.h"
#include "iconset.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QQueue>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOption>
#include <QWidgetAction>

#include <cmath>
#include <optional>

namespace {
struct Item {
    PsiIcon                    *icon  = nullptr;
    const EmojiRegistry::Emoji *emoji = nullptr;

    bool operator==(const Item &other) const
    {
        if (emoji)
            return emoji == other.emoji;
        if (icon != nullptr && other.icon != nullptr)
            return icon->name() == other.icon->name();
        return false;
    }
};
using List = QList<Item>;
}

//----------------------------------------------------------------------------
// IconSelectButton
//----------------------------------------------------------------------------

//! \if _hide_doc_
/**
    \class IconSelectButton
    \brief This button is used by IconSelect and displays one PsiIcon
*/
class IconSelectButton : public QAbstractButton {
    Q_OBJECT

private:
    Item  item_;
    QSize s;
    QSize maxIconSize;
    bool  animated;

public:
    IconSelectButton(QWidget *parent) : QAbstractButton(parent)
    {
        item_    = {};
        animated = false;
    }

    ~IconSelectButton()
    {
        iconStop();

        if (item_.icon) {
            delete item_.icon;
            item_.icon = nullptr;
        }
    }

    // const PsiIcon *icon() const { return ic; }

    void setItem(const Item &newItem, const QSize &maxSize = {})
    {
        iconStop();
        if (item_.icon) {
            delete item_.icon;
        }
        item_       = newItem;
        maxIconSize = maxSize;
        if (item_.icon) {
            item_.icon = new PsiIcon(*(const_cast<PsiIcon *>(item_.icon)));
        } else {
            setText(item_.emoji->code);
            setToolTip(item_.emoji->name);
        }
    }

    const Item &item() const { return item_; }

    QSize sizeHint() const { return s; }
    void  setSizeHint(QSize sh) { s = sh; }

signals:
    void iconSelected(const PsiIcon *);
    void textSelected(QString);

private:
    void iconStart()
    {
        if (item_.icon) {
            connect(item_.icon, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
            if (!animated) {
                item_.icon->activated(false);
                animated = true;
            }

            if (!item_.icon->text().isEmpty()) {
                // and list of possible variants in the ToolTip
                QStringList toolTip;
                for (const PsiIcon::IconText &t : item_.icon->text()) {
                    toolTip += t.text;
                }

                QString toolTipText = toolTip.join(", ");
                if (toolTipText.length() > 30)
                    toolTipText = toolTipText.left(30) + "...";

                setToolTip(toolTipText);
            }
        }
    }

    void iconStop()
    {
        if (item_.icon) {
            disconnect(item_.icon, nullptr, this, nullptr);
            if (animated) {
                item_.icon->stop();
                animated = false;
            }
        }
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEvent *)
#else
    void enterEvent(QEnterEvent *)
#endif
    {
        iconStart();
        setFocus();
        update();
    } // focus follows mouse mode
    void leaveEvent(QEvent *)
    {
        iconStop();
        clearFocus();
        update();
    }

private slots:
    void iconUpdated() { update(); }

private:
    // reimplemented
    void paintEvent(QPaintEvent *)
    {
        QPainter                  p(this);
        QFlags<QStyle::StateFlag> flags = QStyle::State_Active | QStyle::State_Enabled;

        if (hasFocus())
            flags |= QStyle::State_Selected;

        QStyleOptionMenuItem opt;
        opt.palette = palette();
        opt.state   = flags;
        opt.font    = font();
        opt.rect    = rect();
        style()->drawControl(QStyle::CE_MenuItem, &opt, &p, this);

        if (item_.icon) {
            QPixmap pix = item_.icon->pixmap(maxIconSize);
            if (pix.width() > maxIconSize.width() || pix.height() > maxIconSize.height()) {
                pix = pix.scaled(maxIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            p.drawPixmap((width() - pix.width()) / 2, (height() - pix.height()) / 2, pix);
        } else {
            p.drawText(opt.rect, Qt::AlignVCenter | Qt::AlignCenter, text());
        }
    }
};
//! \endif

//----------------------------------------------------------------------------
// IconSelect -- the widget that does all dirty work
//----------------------------------------------------------------------------

class IconSelect : public QWidget {
    Q_OBJECT

private:
    IconSelectPopup *menu;

    Iconset             is;
    std::optional<List> icons_; // when set this will rendered instead of Iconset

    std::optional<int> rowSize_; // explicit row size in columns

    QGridLayout *grid;
    QString      titleFilter;
    bool         shown;
    bool         emojiSorting;

signals:
    void updatedGeometry();
    void selected(IconSelectButton *);

public:
    IconSelect(IconSelectPopup *parentMenu);
    ~IconSelect();

    void                  setIconset(const Iconset &);
    inline const Iconset &iconset() const { return is; }

    void                              setIcons(const List &);
    inline const std::optional<List> &icons() const { return icons_; }

    inline void setEmojiSortingEnabled(bool enabled) { emojiSorting = enabled; }
    void        setTitleFilter(const QString &title);

    inline void               setRowSize(int rs) { rowSize_ = rs; }
    inline std::optional<int> rowSize() const { return rowSize_; }

protected:
    QList<PsiIcon *>         sortEmojis() const;
    void                     noIcons();
    void                     createLayout();
    void                     updateGrid();
    std::pair<QSizeF, QSize> computeIconSize() const;

    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);

        QStyleOptionMenuItem opt;
        opt.palette = palette();
        opt.rect    = rect();
        style()->drawControl(QStyle::CE_MenuEmptyArea, &opt, &p, this);
    }

protected slots:
    void closeMenu();
};

IconSelect::IconSelect(IconSelectPopup *parentMenu) : QWidget(parentMenu)
{
    menu = parentMenu;
    connect(menu, SIGNAL(textSelected(QString)), SLOT(closeMenu()));

    grid = nullptr;
    noIcons();

    Q_UNUSED(shown)
}

IconSelect::~IconSelect() { }

void IconSelect::closeMenu()
{
    // this way all parent menus (if any) would be closed too
    QMouseEvent me(QEvent::MouseButtonPress, menu->pos() - QPoint(5, 5), QCursor::pos(), Qt::LeftButton, Qt::LeftButton,
                   Qt::NoModifier);
    menu->mousePressEvent(&me);

    // just in case
    menu->close();
}

void IconSelect::createLayout()
{
    Q_ASSERT(!grid);
    grid        = new QGridLayout(this);
    auto margin = style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, this);
    grid->setContentsMargins(margin, margin, margin, margin);
    grid->setSpacing(1);
}

void IconSelect::noIcons()
{
    createLayout();
    QLabel *lbl = new QLabel(this);
    grid->addWidget(lbl, 0, 0);
    lbl->setText(tr("No icons available"));
    emit updatedGeometry();
}

void IconSelect::setIconset(const Iconset &iconset)
{
    rowSize_ = {};
    is       = iconset;
    shown    = false; // we need to recompute geometry
    updateGrid();
}

void IconSelect::setIcons(const List &l)
{
    // rowSize_ = {};
    icons_ = l;
    shown  = false;
    updateGrid();
}

void IconSelect::updateGrid()
{
    blockSignals(true);
    // delete all children
    if (grid) {
        delete grid;
        grid = nullptr;

        QObjectList list = children();
        qDeleteAll(list);
    }

    // first we need to find optimal size for elements and don't forget about
    // taking too much screen space
    auto [iconSize, maxPrefSize] = computeIconSize();
    List toRender;
    if (icons_.has_value()) {
        toRender = *icons_;
    } else if (is.count() > 0) {
        toRender.reserve(is.count());
        std::transform(is.begin(), is.end(), std::back_inserter(toRender), [](auto icon) { return Item { icon }; });
    } else {
        for (auto const &emoji : EmojiRegistry::instance()) {
            if (titleFilter.isEmpty() || emoji.name.contains(titleFilter)) {
                toRender.append(Item { nullptr, &emoji });
                if (!titleFilter.isEmpty() && toRender.size() == 40) {
                    break;
                }
            }
        }
    }

    const int margin   = 2;
    int       tileSize = int(qMax(iconSize.width(), iconSize.height())) + 2 * margin;

    QRect r       = menu->screen()->availableGeometry();
    int   maxSize = qMin(r.width(), r.height()) / 3;

    if (!rowSize_.has_value()) {
        rowSize_       = int(ceil(std::sqrt(double(toRender.size()))));
        int maxColumns = int(maxSize / tileSize);
        rowSize_       = *rowSize_ > maxColumns ? maxColumns : *rowSize_;
    }

    // now, fill grid with elements
    createLayout();

    int row    = 0;
    int column = 0;

    // make emoji font
    auto font = qApp->font();
    if (font.pointSize() == -1)
        font.setPixelSize(font.pixelSize() * 2.5);
    else
        font.setPointSize(font.pointSize() * 2.5);
#if defined(Q_OS_WIN)
    font.setFamily("Segoe UI Emoji");
#elif defined(Q_OS_MAC)
    font.setFamily("Apple Color Emoji");
#else
    font.setFamily("Noto Color Emoji");
#endif

    for (auto const &item : toRender) {
        IconSelectButton *b = new IconSelectButton(this);
        b->setFont(font);
        b->setItem(item, maxPrefSize);
        b->setSizeHint(QSize(tileSize, tileSize));
        connect(b, &QAbstractButton::clicked, this, [b, this]() { emit selected(b); });
        grid->addWidget(b, row, column);

        if (++column >= *rowSize_) {
            ++row;
            column = 0;
        }
    }

    blockSignals(false);
    if (!shown) {
        shown = true;
        emit updatedGeometry();
    }
}

std::pair<QSizeF, QSize> IconSelect::computeIconSize() const
{
    QSizeF iconSize;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const auto fontSz = qApp->fontMetrics().height();
#else
    const auto fontSz = QFontMetrics(qApp->font()).height();
#endif
    int  maxPrefTileHeight = fontSz * 3;
    auto maxPrefSize       = QSize(maxPrefTileHeight, maxPrefTileHeight);

    auto scaledIconSize = [&maxPrefSize](auto icon) {
        auto pix     = icon->pixmap(maxPrefSize);
        auto pixSize = pix.size();
        if (pix.width() > maxPrefSize.width() || pix.height() > maxPrefSize.height()) {
            pixSize.scale(maxPrefSize, Qt::KeepAspectRatio);
        }
        return pixSize;
    };

    if (is.count() > 0) {
        for (auto const icon : is) {
            iconSize += scaledIconSize(icon);
        }
        iconSize /= is.count();
    } else if (icons_.has_value()) {
        int cnt = 0;
        for (auto const &icon : std::as_const(*icons_)) {
            if (icon.icon) {
                iconSize += scaledIconSize(icon.icon);
                cnt++;
            }
        }
        if (cnt) {
            iconSize /= cnt;
        }
    }
    if (iconSize.isEmpty()) {
        iconSize = { fontSz * 2.5, fontSz * 2.5 };
    }
    return { iconSize, maxPrefSize };
}

void IconSelect::setTitleFilter(const QString &title)
{
    titleFilter = title;
    updateGrid();
}

QList<PsiIcon *> IconSelect::sortEmojis() const
{
    QList<PsiIcon *> ret;
    QList<PsiIcon *> notEmoji;
    ret.reserve(is.count());
    QHash<QString, PsiIcon *> cp2icon; // codepoint to icon map

    auto &er = EmojiRegistry::instance();

    for (const auto &icon : is) {
        bool found = false;
        for (const auto &text : icon->text()) {
            if (er.isEmoji(text.text)) {
                cp2icon.insert(text.text, icon);
                found = true;
                break;
            }
        }
        if (!found)
            notEmoji.append(icon);
    }

    for (auto const &group : EmojiRegistry::instance().groups) {
        for (auto const &subgroup : group.subGroups) {
            for (auto const &emoji : subgroup.emojis) {
                auto icon = cp2icon.value(emoji.code);
                if (icon) {
                    ret.append(icon);
                }
            }
        }
    }

    ret += notEmoji;
    return ret;
}

//----------------------------------------------------------------------------
// IconSelectPopup
//----------------------------------------------------------------------------

class IconSelectPopup::Private : public QObject {
    Q_OBJECT

public:
    Private(IconSelectPopup *parent) : QObject(parent), parent_(parent) { }
    ~Private()
    {
        for (auto const &item : recent) {
            if (item.icon) {
                delete item.icon;
            }
        }
    }

    IconSelectPopup *parent_;

    Iconset        recentIconset;
    IconSelect    *recentSel_    = nullptr;
    QWidgetAction *recentAction_ = nullptr;
    // QScrollArea   *recentScrollArea_ = nullptr;

    IconSelect    *emotsSel_        = nullptr;
    QWidgetAction *emotsAction_     = nullptr;
    QScrollArea   *emotsScrollArea_ = nullptr;

    ActionLineEdit *findBar_    = nullptr;
    IconAction     *findAct_    = nullptr;
    QWidgetAction  *findAction_ = nullptr;
    List            recent;

public slots:
    void updatedGeometry()
    {
        emotsAction_->setDefaultWidget(emotsScrollArea_);
        QRect r         = emotsScrollArea_->screen()->availableGeometry();
        int   maxSize   = qMin(r.width(), r.height()) / 3;
        int   vBarWidth = emotsScrollArea_->verticalScrollBar()->isEnabled()
              ? emotsScrollArea_->verticalScrollBar()->sizeHint().rwidth()
              : 0;
        emotsScrollArea_->setMinimumWidth(emotsSel_->sizeHint().rwidth() + vBarWidth);
        emotsScrollArea_->setMinimumHeight(qMin(emotsSel_->sizeHint().rheight(), maxSize));
        emotsScrollArea_->setFrameStyle(QFrame::Plain);

        recentSel_->setRowSize(*emotsSel_->rowSize());
        recentAction_->setDefaultWidget(recentSel_);

        parent_->removeAction(emotsAction_);
        parent_->removeAction(recentAction_);
        parent_->removeAction(findAction_);

        parent_->addAction(findAction_);
        parent_->addAction(recentAction_);
        parent_->addAction(emotsAction_);
        findBar_->setFocus();
    }

    void selected(IconSelectButton *btn)
    {
        btn->clearFocus();

        auto const &item = btn->item();
        auto        it   = std::find_if(recent.begin(), recent.end(), [&item](auto const &r) { return item == r; });

        auto rotated = false;
        if (it != recent.end()) {
            auto idx = std::distance(recent.begin(), it);
            if (idx != 0) {
                std::rotate(recent.begin(), recent.begin() + idx, recent.begin() + idx + 1);
                rotated = true;
            }
        } else {
            auto copyItem = item;
            if (copyItem.icon) {
                copyItem.icon = new PsiIcon(*copyItem.icon);
            }
            recent.push_front(copyItem);
            if (recent.size() > *emotsSel_->rowSize()) {
                if (recent.back().icon) {
                    delete recent.back().icon;
                }
                recent.pop_back();
            }
            rotated = true;
        }

        if (item.icon) {
            emit parent_->iconSelected(item.icon);
            emit parent_->textSelected(item.icon->defaultText());
        } else {
            emit parent_->textSelected(btn->text());
        }

        if (rotated) {
            recentSel_->setIcons(recent);
        }
    }

    void setTitleFilter(const QString &filter) { emotsSel_->setTitleFilter(filter); }
};

IconSelectPopup::IconSelectPopup(QWidget *parent) : QMenu(parent), d(new Private(this))
{
    d->findAction_ = new QWidgetAction(this);
    d->findBar_    = new ActionLineEdit(this);
    d->findAct_    = new IconAction(d->findBar_);
    d->findAct_->setPsiIcon("psi/search");
    d->findBar_->addAction(d->findAct_);
    d->findAction_->setDefaultWidget(d->findBar_);
    connect(d->findBar_, &QLineEdit::textChanged, d, &Private::setTitleFilter);

    d->recentSel_    = new IconSelect(this);
    d->recentAction_ = new QWidgetAction(this);
    connect(d->recentSel_, &IconSelect::updatedGeometry, d, &IconSelectPopup::Private::updatedGeometry);
    connect(d->recentSel_, &IconSelect::selected, d, &IconSelectPopup::Private::selected);

    d->emotsSel_        = new IconSelect(this);
    d->emotsAction_     = new QWidgetAction(this);
    d->emotsScrollArea_ = new QScrollArea(this);
    d->emotsScrollArea_->setWidget(d->emotsSel_);
    d->emotsScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->emotsScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->emotsScrollArea_->setWidgetResizable(true);
    connect(d->emotsSel_, &IconSelect::updatedGeometry, d, &IconSelectPopup::Private::updatedGeometry);
    connect(d->emotsSel_, &IconSelect::selected, d, &IconSelectPopup::Private::selected);

    d->updatedGeometry();
}

IconSelectPopup::~IconSelectPopup() { }

void IconSelectPopup::setIconset(const Iconset &i)
{
    std::remove_if(d->recent.begin(), d->recent.end(), [](auto const &item) { return item.icon != nullptr; });
    d->recentSel_->setIcons(d->recent);
    d->emotsSel_->setIconset(i);
}

const Iconset &IconSelectPopup::iconset() const { return d->emotsSel_->iconset(); }

void IconSelectPopup::setEmojiSortingEnabled(bool enabled) { d->emotsSel_->setEmojiSortingEnabled(enabled); }

/**
    It's used by child widget to close the menu by simulating a
    click slightly outside of menu. This seems to be the best way
    to achieve this.
*/
void IconSelectPopup::mousePressEvent(QMouseEvent *e) { QMenu::mousePressEvent(e); }

#include "iconselect.moc"
