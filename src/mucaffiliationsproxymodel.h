#ifndef MUCAFFILIATIONSPROXYMODEL_H
#define MUCAFFILIATIONSPROXYMODEL_H

#include <QSortFilterProxyModel>

class MUCAffiliationsProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    MUCAffiliationsProxyModel(QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
};

#endif // MUCAFFILIATIONSPROXYMODEL_H
