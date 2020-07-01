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

#include "emojiregistry.h"
#include "iconset.h"
#include "psitooltip.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOption>
#include <QTextCodec>
#include <QWidgetAction>

#include <cmath>

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
    PsiIcon *ic;
    QSize    s;
    QSize    maxIconSize;
    bool     animated;

public:
    IconSelectButton(QWidget *parent) : QAbstractButton(parent)
    {
        ic       = nullptr;
        animated = false;
        connect(this, SIGNAL(clicked()), SLOT(iconSelected()));
    }

    ~IconSelectButton()
    {
        iconStop();

        if (ic) {
            delete ic;
            ic = nullptr;
        }
    }

    void setIcon(const PsiIcon *i, const QSize &maxSize = QSize())
    {
        iconStop();

        if (ic) {
            delete ic;
            ic = nullptr;
        }

        maxIconSize = maxSize;
        if (i)
            ic = new PsiIcon(*(const_cast<PsiIcon *>(i)));
        else
            ic = nullptr;
    }

    const PsiIcon *icon() const { return ic; }

    QSize sizeHint() const { return s; }
    void  setSizeHint(QSize sh) { s = sh; }

signals:
    void iconSelected(const PsiIcon *);
    void textSelected(QString);

private:
    void iconStart()
    {
        if (ic) {
            connect(ic, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
            if (!animated) {
                ic->activated(false);
                animated = true;
            }

            if (!ic->text().isEmpty()) {
                // and list of possible variants in the ToolTip
                QStringList toolTip;
                for (PsiIcon::IconText t : ic->text()) {
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
        if (ic) {
            disconnect(ic, nullptr, this, nullptr);
            if (animated) {
                ic->stop();
                animated = false;
            }
        }
    }

    void enterEvent(QEvent *)
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

    void iconSelected()
    {
        clearFocus();
        if (ic) {
            emit iconSelected(ic);
            emit textSelected(ic->defaultText());
        }
    }

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

        if (ic) {
            QPixmap pix = ic->pixmap(maxIconSize);
            if (pix.width() > maxIconSize.width() || pix.height() > maxIconSize.height()) {
                pix = pix.scaled(maxIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            p.drawPixmap((width() - pix.width()) / 2, (height() - pix.height()) / 2, pix);
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
    Iconset          is;
    QGridLayout *    grid;
    bool             shown;
    bool             emojiSorting;

signals:
    void updatedGeometry();

public:
    IconSelect(IconSelectPopup *parentMenu);
    ~IconSelect();

    void           setIconset(const Iconset &);
    const Iconset &iconset() const;

    void setEmojiSortingEnabled(bool enabled);

protected:
    QList<PsiIcon *> sortEmojis() const;
    void             noIcons();
    void             createLayout();

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
    QMouseEvent me(QEvent::MouseButtonPress, menu->pos() - QPoint(5, 5), Qt::LeftButton, Qt::LeftButton,
                   Qt::NoModifier);
    menu->mousePressEvent(&me);

    // just in case
    menu->close();
}

void IconSelect::createLayout()
{
    Q_ASSERT(!grid);
    grid = new QGridLayout(this);
    grid->setMargin(style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, this));
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
    is = iconset;

    // delete all children
    if (grid) {
        delete grid;
        grid = nullptr;

        QObjectList list = children();
        qDeleteAll(list);
    }

    if (!is.count()) {
        noIcons();
        return;
    }

    // first we need to find optimal size for elements and don't forget about
    // taking too much screen space
    float w = 0, h = 0;
    int   maxPrefTileHeight = fontInfo().pixelSize() * 2;
    auto  maxPrefSize       = QSize(maxPrefTileHeight, maxPrefTileHeight);

    double                   count; // the 'double' type is somewhat important for MSVC.NET here
    QListIterator<PsiIcon *> it = is.iterator();
    for (count = 0; it.hasNext(); count++) {
        PsiIcon *icon    = it.next();
        auto     pix     = icon->pixmap(maxPrefSize);
        auto     pixSize = pix.size();
        if (pix.width() > maxPrefSize.width() || pix.height() > maxPrefSize.height()) {
            pixSize.scale(maxPrefSize, Qt::KeepAspectRatio);
        }
        w += pixSize.width();
        h += pixSize.height();
    }
    w /= float(count);
    h /= float(count);

    const int margin   = 2;
    int       tileSize = int(qMax(w, h)) + 2 * margin;

    QRect r       = QApplication::desktop()->availableGeometry(menu);
    int   maxSize = qMin(r.width(), r.height()) / 3;

    int size       = int(ceil(std::sqrt(count)));
    int maxColumns = int(maxSize / tileSize) - 1;
    size           = size > maxColumns ? maxColumns : size;

    // now, fill grid with elements
    createLayout();
    count = 0;

    int row    = 0;
    int column = 0;

    QList<PsiIcon *> sortIcons;
    if (emojiSorting) {
        sortIcons = sortEmojis();
        it        = QListIterator<PsiIcon *>(sortIcons);
    } else
        it = is.iterator();
    while (it.hasNext()) {
        IconSelectButton *b = new IconSelectButton(this);
        grid->addWidget(b, row, column);
        b->setIcon(it.next(), maxPrefSize);
        b->setSizeHint(QSize(tileSize, tileSize));
        connect(b, SIGNAL(iconSelected(const PsiIcon *)), menu, SIGNAL(iconSelected(const PsiIcon *)));
        connect(b, &IconSelectButton::textSelected, menu, &IconSelectPopup::textSelected);

        if (++column >= size) {
            ++row;
            column = 0;
        }
    }
    emit updatedGeometry();
}

const Iconset &IconSelect::iconset() const { return is; }

void IconSelect::setEmojiSortingEnabled(bool enabled) { emojiSorting = enabled; }

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
    Private(IconSelectPopup *parent) : QObject(parent), parent_(parent), icsel_(nullptr), widgetAction_(nullptr) { }

    IconSelectPopup *parent_;
    IconSelect *     icsel_;
    QWidgetAction *  widgetAction_;
    QScrollArea *    scrollArea_;

public slots:
    void updatedGeometry()
    {
        widgetAction_->setDefaultWidget(scrollArea_);
        QRect r       = QApplication::desktop()->availableGeometry(scrollArea_);
        int   maxSize = qMin(r.width(), r.height()) / 3;
        int   vBarWidth
            = scrollArea_->verticalScrollBar()->isEnabled() ? scrollArea_->verticalScrollBar()->sizeHint().rwidth() : 0;
        scrollArea_->setMinimumWidth(icsel_->sizeHint().rwidth() + vBarWidth);
        scrollArea_->setMinimumHeight(qMin(icsel_->sizeHint().rheight(), maxSize));
        parent_->removeAction(widgetAction_);
        parent_->addAction(widgetAction_);
    }
};

IconSelectPopup::IconSelectPopup(QWidget *parent) : QMenu(parent)
{
    d                = new Private(this);
    d->icsel_        = new IconSelect(this);
    d->widgetAction_ = new QWidgetAction(this);
    d->scrollArea_   = new QScrollArea(this);
    d->scrollArea_->setWidget(d->icsel_);
    d->scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->scrollArea_->setWidgetResizable(true);
    connect(d->icsel_, &IconSelect::updatedGeometry, d, &IconSelectPopup::Private::updatedGeometry);
    d->updatedGeometry();
}

IconSelectPopup::~IconSelectPopup() { }

void IconSelectPopup::setIconset(const Iconset &i) { d->icsel_->setIconset(i); }

const Iconset &IconSelectPopup::iconset() const { return d->icsel_->iconset(); }

void IconSelectPopup::setEmojiSortingEnabled(bool enabled) { d->icsel_->setEmojiSortingEnabled(enabled); }

/**
    It's used by child widget to close the menu by simulating a
    click slightly outside of menu. This seems to be the best way
    to achieve this.
*/
void IconSelectPopup::mousePressEvent(QMouseEvent *e) { QMenu::mousePressEvent(e); }

#include "iconselect.moc"
