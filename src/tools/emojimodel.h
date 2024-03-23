#ifndef EMOJIMODEL_H
#define EMOJIMODEL_H

#include <QAbstractItemModel>

class EmojiModel : public QAbstractItemModel {
public:
    explicit EmojiModel(QObject *parent = nullptr);

protected:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    int  rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int  columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};

#endif // EMOJIMODEL_H
