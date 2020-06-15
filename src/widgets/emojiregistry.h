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

private:
    EmojiRegistry();
    EmojiRegistry(const EmojiRegistry &) = delete;
    EmojiRegistry &operator=(const EmojiRegistry &) = delete;

    const std::map<quint32, quint32> ranges_; // start to end range mapping
};

#endif // EMOJIREGISTRY_H
