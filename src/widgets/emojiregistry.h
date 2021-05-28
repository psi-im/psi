/*
 * emojiregistry.h - A registry of all standard Emoji
 * Copyright (C) 2020  Sergey Ilinykh
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

#ifndef EMOJIREGISTRY_H
#define EMOJIREGISTRY_H

#include <QString>

#include <map>
#include <vector>

class EmojiRegistry {
public:
    enum class Category { None, Emoji, SkinTone, ZWJ, FullQualify, SimpleKeycap };

    struct Emoji {
        const QString code;
        const QString name; // latin1
    };

    struct SubGroup {
        const QString            name;
        const std::vector<Emoji> emojis;
    };

    struct Group {
        const QString               name;
        const std::vector<SubGroup> subGroups;
    };

    const std::vector<Group> groups;

    static const EmojiRegistry &instance();

    // const QList<Group> &groups() const { return groups_; }
    bool isEmoji(const QString &code) const;

    /// Find emoji in a string starting from the specified position
    QStringRef findEmoji(const QString &in, int startPos = 0) const;

    /*!
     * \brief startCategory returns category of what the string starts with if the sequence looks valida for emoji
     * \param in
     * \return category
     */
    Category startCategory(QStringRef in) const;
    int      count() const;

    struct iterator {
        int group_idx    = 0;
        int subgroup_idx = 0;
        int emoji_idx    = 0;

        inline bool operator!=(const iterator &other) const
        {
            return other.group_idx != group_idx || other.subgroup_idx != subgroup_idx || other.emoji_idx != emoji_idx;
        }

        iterator &                  operator++();
        const EmojiRegistry::Emoji &operator*() const
        {
            return EmojiRegistry::instance().groups[group_idx].subGroups[subgroup_idx].emojis[emoji_idx];
        }
    };

    inline iterator begin() const { return {}; }
    inline iterator end() const { return { int(EmojiRegistry::instance().groups.size()), 0, 0 }; }

private:
    EmojiRegistry();
    EmojiRegistry(const EmojiRegistry &) = delete;
    EmojiRegistry &operator=(const EmojiRegistry &) = delete;

    const std::map<quint32, quint32> ranges_; // start to end range mapping
};

#endif // EMOJIREGISTRY_H
