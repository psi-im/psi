#include "emojimodel.h"

#include "widgets/emojiregistry.h"

#include <unordered_map>

// Internal ID logic
// 0 - invalid id / default id. So valid start with 1 (we always add 1)
// Below is ID - 1:
// 0xXXFFFFFF - Refers group itself.
// 0xXXXXXXXX - Refers emoji

// An idea of fast emoji lookup
// Terms:
//   EmojiRef - a refercence to emoji uint32(8 bit group index, 8 bit subgroup index, 16 bit emoji index)
// We have ranges already. The idea is to attach to every range an EmojiRef. So first code in the range
// will correspond to this EmojiRef, range.start + 1 will correspond to EmojiRef + 1 and so on.
// Since a range may refer multiple groups/subgroups, then we need more ranges, so only one consequent
// subgroup with consequent emojis (next emoji code = prev + 1) can be referred at time.
// Emoji ranges with less than 3 emojis can instead added to a hash table to make both lookups faster.
// Lookup:
// First we lookup the hash table, because early and the most used emojis were spread quite randomly,
// so there is a big chance to find there and it's quick.
// If not found then do binary search in the ranges array.

class EmojiModel::Private : public QObject {
    Q_OBJECT
public:
    using EmojiRef = std::uint32_t;

    EmojiModel::Options                     options;
    Iconset                                 iconset;
    std::unordered_map<EmojiRef, PsiIcon *> emoji2icon;

    struct MergedIcon {
        std::uint32_t emojiRef = 0; // if not 0 then valid
        PsiIcon      *icon     = nullptr;
    };

    struct Group {
        QString           groupName;
        QList<MergedIcon> icons;
    };
#if 0
    QList<PsiIcon *> sortEmojis()
    {
        Group sortedIconset;
        Group unsortedIconset;
        sortedIconset.icons.reserve(iconset.count());
        QHash<QString, PsiIcon *> cp2icon; // codepoint to icon map

        auto &er = EmojiRegistry::instance();

        for (const auto &icon : iconset) {
            auto const &codes  = icon->text();
            auto        result = er.classify(txt.text);
            if (result) {
                emoji2icon.insert(*result, icon);
            }
            if (auto it
                = std::find_if(codes.begin(), codes.end(), [&er](auto const &txt) { return er.isEmoji(txt.text); });
                it != codes.end()) {
                cp2icon.insert(it->text, icon);
            } else {
                unsortedIconset.append(icon);
            }
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
#endif
    void update()
    {
        // reread iconset, resort, merge with fonts when enabled
        if (options & EmojiModel::EmojiSorting) {
            // sortedIconset = sortEmojis();
        }
#if 0
        if (grid) {
            delete grid;
            grid = nullptr;

            QObjectList list = children();
            qDeleteAll(list);
        }

        bool fontEmojiMode = is.count() == 0;

        // first we need to find optimal size for elements and don't forget about
        // taking too much screen space
        float w = 0, h = 0;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const auto fontSz = qApp->fontMetrics().height();
#else
        const auto fontSz = QFontMetrics(qApp->font()).height();
#endif
        int  maxPrefTileHeight = fontSz * 3;
        auto maxPrefSize       = QSize(maxPrefTileHeight, maxPrefTileHeight);

        double                              count; // the 'double' type is somewhat important for MSVC.NET here
        QList<const EmojiRegistry::Emoji *> emojis;
        if (fontEmojiMode) {
            for (auto const &emoji : EmojiRegistry::instance()) {
                if (titleFilter.isEmpty() || emoji.name.contains(titleFilter)) {
                    emojis.append(&emoji);
                }
            }

            count = emojis.count();
            w     = fontSz * 2.5;
            h     = fontSz * 2.5;
        } else {
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
        }

        const int margin   = 2;
        int       tileSize = int(qMax(w, h)) + 2 * margin;

        QRect r       = menu->screen()->availableGeometry();
        int   maxSize = qMin(r.width(), r.height()) / 3;

        int size       = int(ceil(std::sqrt(count)));
        int maxColumns = int(maxSize / tileSize);
        size           = size > maxColumns ? maxColumns : size;

        // now, fill grid with elements
        createLayout();

        int row    = 0;
        int column = 0;

        if (fontEmojiMode) {
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
            for (auto const &emoji : emojis) {
                IconSelectButton *b = new IconSelectButton(this);
                b->setFont(font);
                grid->addWidget(b, row, column);
                b->setText(emoji->code);
                b->setToolTip(emoji->name);
                b->setSizeHint(QSize(tileSize, tileSize));
                connect(b, qOverload<const PsiIcon *>(&IconSelectButton::iconSelected), menu,
                        &IconSelectPopup::iconSelected);
                connect(b, &IconSelectButton::textSelected, menu, &IconSelectPopup::textSelected);

                if (++column >= size) {
                    ++row;
                    column = 0;
                }
            }
        } else {
            QListIterator<PsiIcon *> it = is.iterator();
            QList<PsiIcon *>         sortIcons;
            if (emojiSorting) {
                sortIcons = sortEmojis();
                it        = QListIterator<PsiIcon *>(sortIcons);
            }
            while (it.hasNext()) {
                IconSelectButton *b = new IconSelectButton(this);
                grid->addWidget(b, row, column);
                b->setIcon(it.next(), maxPrefSize);
                b->setSizeHint(QSize(tileSize, tileSize));
                connect(b, qOverload<const PsiIcon *>(&IconSelectButton::iconSelected), menu,
                        &IconSelectPopup::iconSelected);
                connect(b, &IconSelectButton::textSelected, menu, &IconSelectPopup::textSelected);

                if (++column >= size) {
                    ++row;
                    column = 0;
                }
            }
        }
        emit updatedGeometry();
#endif
    }
};

EmojiModel::EmojiModel(Options options, const Iconset &iconset, QObject *parent) :
    QAbstractItemModel { parent }, d(new Private)
{
    if (!options) {
        options = UseFont | UseIconset | EmojiSorting | PreferIconset;
    }
    d->options = options;
    d->iconset = iconset;
    d->update();
}

EmojiModel::~EmojiModel() = default;

void EmojiModel::setIconset(const Iconset &iconset)
{
    beginResetModel();
    d->iconset = iconset;
    d->update();
    endResetModel();
}

void EmojiModel::setOptions(Options options)
{
    beginResetModel();
    d->options = options;
    d->update();
    endResetModel();
}

const Iconset &EmojiModel::iconset() const { return d->iconset; }

QModelIndex EmojiModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0) {
        return QModelIndex();
    }

    if (parent.isValid()) {                                    // selecting emojis in a group
        auto        groupInternalId = parent.internalId() - 1; // see +1 below
        auto        groupId         = groupInternalId >> 24;
        auto const &group           = EmojiRegistry::instance().groups[groupId];

        std::size_t relColumn = column;
        for (auto const &subGroup : group.subGroups) {
            if (relColumn < subGroup.emojis.size()) {
                auto subGroupId      = &subGroup - &group.subGroups[0];
                auto emojiInternalId = (groupInternalId & 0xff000000) | (subGroupId << 16) | relColumn;
                return createIndex(row, column, emojiInternalId + 1); // 0 is reserved for default/invalid
            }
            relColumn -= subGroup.emojis.size();
        }
    } else {
        auto const &groups = EmojiRegistry::instance().groups;
        quintptr    rowx   = row;
        if (rowx < groups.size()) {
            return createIndex(row, column, (rowx << 24) | 0x00ffffff + 1); // 0x00ffffff - refers group itself
        }
    }
    return QModelIndex();
}

QModelIndex EmojiModel::parent(const QModelIndex &child) const
{
    auto id = child.internalId();
    if (!id) {
        qWarning("invalid child passed to EmojiModel::parent()");
        return QModelIndex();
    }
    id--;
    if ((id & 0x00ffffff) == 0x00ffffff) { // referring group itself
        return QModelIndex();
    }
    auto groupIndex = id >> 24;
    return createIndex(groupIndex, 0, id | 0x00ffffff);
}

int EmojiModel::rowCount(const QModelIndex &parent) const
{
    auto id = parent.internalId();
    if (!id) { // seems root
        return int(EmojiRegistry::instance().groups.size());
    }
    id--;
    if ((id & 0x00ffffff) == 0x00ffffff) {
        int         sum   = 0;
        auto const &group = EmojiRegistry::instance().groups[id >> 24];
        for (auto const &subGroup : group.subGroups) {
            sum += subGroup.emojis.size();
        }
        return sum;
    }
    return 0;
}

int EmojiModel::columnCount(const QModelIndex &parent) const
{
    auto id = parent.internalId();
    if (!id || (id & 0x00ffffff) == 0) {
        return 1;
    }
    return 0;
}

bool EmojiModel::hasChildren(const QModelIndex &parent) const
{
    auto id = parent.internalId();
    return !id || (id & 0x00ffffff) == 0;
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const
{
    auto        groupInternalId = index.internalId() - 1; // see +1 below
    auto        groupId         = groupInternalId >> 24;
    auto const &group           = EmojiRegistry::instance().groups[groupId];
    if ((groupInternalId & 0x00ffffff) == 0x00ffffff) {
        if (role == Qt::DisplayRole) {
            return group.name;
        }
        return {};
    }
    auto const &subGroup = group.subGroups[(groupInternalId >> 16) & 0xff];
    auto const &emoji    = subGroup.emojis[groupInternalId & 0xffff];
    if (role == Qt::DisplayRole) {
        return emoji.code;
    }
    return {};
}

#include "emojimodel.moc"
