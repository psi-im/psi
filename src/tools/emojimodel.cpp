#include "emojimodel.h"
#include "widgets/emojiregistry.h"

// Internal ID logic
// 0 - invalid id / default id. So valid start with 1 (we always add 1)
// Below is ID - 1:
// 0xXXFFFFFF - Refers group itself.
// 0xXXXXXXXX - Refers emoji

EmojiModel::EmojiModel(QObject *parent) : QAbstractItemModel { parent } { }

QModelIndex EmojiModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0) {
        return QModelIndex();
    }

    if (parent.isValid()) {                                    // selecting emojis in a group
        auto        groupInternalId = parent.internalId() - 1; // see +1 below
        auto        groupId         = groupInternalId >> 24;
        auto const &group           = EmojiRegistry::instance().groups[groupId];

        auto relColumn = column;
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
        if (row < groups.size()) {
            return createIndex(row, column, (quintptr(row) << 24) | 0x00ffffff + 1); // 0x00ffffff - refers group itself
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
