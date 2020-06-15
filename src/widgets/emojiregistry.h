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
