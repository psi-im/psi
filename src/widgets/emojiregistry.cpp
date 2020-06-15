#include "emojiregistry.h"
#include "emojidb.cpp"

const EmojiRegistry &EmojiRegistry::instance()
{
    static EmojiRegistry i;
    return i;
}

bool EmojiRegistry::isEmoji(const QString &code) const
{
    if (code.isEmpty())
        return false;
    uint ucs = 0;
    if (code[0].isHighSurrogate()) {
        if (code.length() < 2)
            return false;
        ucs = QChar::surrogateToUcs4(code[0], code[1]);
    } else {
        ucs = code[0].unicode();
        if (ucs < 256 && (code.size() < 2 || code[1].unicode() != 0xfe0f)) {
            return false; // allow only full-qualified emojis from low range
        }
    }
    auto lb = ranges_.lower_bound(ucs);
    if (lb == ranges_.end() || lb->first != ucs) {
        --lb;
        return (ucs > lb->first && ucs <= lb->second); // if this is a final range
    }
    return lb->first == ucs;
}

EmojiRegistry::EmojiRegistry() : groups(std::move(db)), ranges_(std::move(ranges)) { }
