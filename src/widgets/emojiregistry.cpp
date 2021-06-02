/*
 * emojiregistry.cpp - A registry of all standard Emoji
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

#include "emojiregistry.h"
#include "emojidb.cpp"

const EmojiRegistry &EmojiRegistry::instance()
{
    static EmojiRegistry i;
    return i;
}

bool EmojiRegistry::isEmoji(const QString &code) const
{
    auto cat = startCategory(&code);
    return cat == Category::Emoji || cat == Category::SkinTone;
    // TODO check the whole code is emoji. not just start
}

EmojiRegistry::Category EmojiRegistry::startCategory(QStringRef in) const
{
    if (in.isEmpty())
        return Category::None;
    std::uint32_t ucs;
    if (in[0].isHighSurrogate()) {
        if (in.length() == 1)
            return Category::None;
        ucs = QChar::surrogateToUcs4(in[0], in[1]);
    } else {
        ucs = in[0].unicode();
        if (ucs < 256 && (in.length() == 1 || in[1].unicode() != 0xfe0f)) {
            return Category::None; // allow only full-qualified emojis from low range
        }
    }
    if (ucs == 0x200d)
        return Category::ZWJ;
    if (ucs == 0xfe0f)
        return Category::FullQualify;
    if (ucs <= 0x39 && in.length() > 2 && (ucs >= 0x30 || ucs == 0x2a || ucs == 0x23) && in[1].unicode() == 0xfe0f
        && in[2].unicode() == 0x20e3)
        // number, * or #. excludes 10 keycap
        return Category::SimpleKeycap;

    bool found = false;
    auto lb    = ranges_.lower_bound(ucs);
    if (lb == ranges_.end() || lb->first != ucs) {
        --lb;
        found = (ucs > lb->first && ucs <= lb->second); // if this is a final range
    } else
        found = lb->first == ucs;
    if (found) {
        if (ucs >= 0x1f3fb && ucs <= 0x1f3ff)
            return Category::SkinTone;
        return Category::Emoji; // there more cases to review. like emoji tags/flags etc
    }
    return Category::None;
}

int EmojiRegistry::count() const
{
    int count = 0;
    for (auto const &g : groups) {
        for (auto const &s : g.subGroups) {
            count += s.emojis.size();
        }
    }
    return count;
}

QStringRef EmojiRegistry::findEmoji(const QString &in, int idx) const
{
    int emojiStart = -1;

    bool gotEmoji = false;
    bool gotSkin  = false;
    bool gotFQ    = false;
    for (; idx < in.size(); idx++) {
        auto category = startCategory(QStringRef(&in, idx, in.size() - idx));
        if (gotEmoji && category != Category::None) {
            if (category == Category::ZWJ) { // zero-width joiner
                gotEmoji = false;
                gotSkin  = false;
                gotFQ    = false;
            } else if (category == Category::FullQualify) {
                if (gotFQ)
                    break;      // double qualification is an error
                gotSkin = true; // we can't get skin false after fill qualification
                gotFQ   = true;
            } else if (category == Category::SkinTone) {
                if (gotSkin)
                    break; // can't have 2 skin tones in the same time
                gotSkin = true;
            } else
                break; // TODO review other categories when implemented
        } else if (!gotEmoji
                   && (category == Category::Emoji || category == Category::SkinTone
                       || category == Category::SimpleKeycap)) {
            if (emojiStart == -1)
                emojiStart = idx;
            if (category == Category::SimpleKeycap) // keycap is ascii in a box. 3 symbols
                idx += 2;
            else {
                gotEmoji = true;
                if (category == Category::SkinTone) { // if we start from skin modifier then just draw colored rect
                    idx++;
                    break;
                }
            }
        } else if (emojiStart != -1) { // seems got end of emoji sequence
            break;
        }
        if (in[idx].isHighSurrogate())
            idx++;
    }
    return emojiStart == -1 ? QStringRef() : QStringRef(&in, emojiStart, idx - emojiStart);
}

EmojiRegistry::EmojiRegistry() : groups(std::move(db)), ranges_(std::move(ranges)) { }

EmojiRegistry::iterator &EmojiRegistry::iterator::operator++()
{
    auto const &inst = EmojiRegistry::instance();
    if (std::size_t(emoji_idx) < inst.groups[group_idx].subGroups[subgroup_idx].emojis.size() - 1) {
        emoji_idx++;
        return *this;
    }
    emoji_idx = 0;
    if (std::size_t(subgroup_idx) < inst.groups[group_idx].subGroups.size() - 1) {
        subgroup_idx++;
        return *this;
    }
    subgroup_idx = 0;
    group_idx++;
    return *this;
}
