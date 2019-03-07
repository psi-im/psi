#ifndef MULTIFILETRANSFERVIEW_H
#define MULTIFILETRANSFERVIEW_H

#include <QStyledItemDelegate>

class MultiFileTransferDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    // cache for sizeHint
    mutable int fontPixelSize = 0;
    mutable int itemHeight;
    mutable int spacing;
    mutable int textLeft;
    mutable int textTop;
    mutable int speedTop;
    mutable int progressTop;
    mutable int progressHeight;
    mutable QRect iconRect;
    mutable QPixmap progressTexture;

    static void niceUnit(qlonglong n, qlonglong *div, QString *unit);

    static QString roundedNumber(qlonglong n, qlonglong div);

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index);
protected:
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index);

};


#endif // MULTIFILETRANSFERVIEW_H
