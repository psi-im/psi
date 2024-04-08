#ifndef EMOJIMODEL_H
#define EMOJIMODEL_H

#include "iconset.h"

#include <QAbstractItemModel>

class EmojiModel : public QAbstractItemModel {
public:
    enum Option {
        UseFont       = 1, // include all emojis but iconset will be completely ignored
        UseIconset    = 2,
        EmojiSorting  = 4,
        PreferIconset = 8, // prefer icons from iconset when available in both emojidb and iconset
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit EmojiModel(Options options = {}, const Iconset &iconset = {}, QObject *parent = nullptr);
    ~EmojiModel();

    void           setOptions(Options options);
    void           setIconset(const Iconset &);
    const Iconset &iconset() const;

protected:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    int  rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int  columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    class Private;
    std::unique_ptr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EmojiModel::Options)

#endif // EMOJIMODEL_H
