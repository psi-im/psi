#include "mucaffiliationsproxymodel.h"

MUCAffiliationsProxyModel::MUCAffiliationsProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    sort(0, Qt::AscendingOrder);
    setDynamicSortFilter(true);
}

bool MUCAffiliationsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
        return false;

    if (!idx.parent().isValid())
        return true;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return filterRegExp().indexIn(idx.data().toString()) >= 0;
#else
    return filterRegularExpression().match(idx.data().toString()).hasMatch();
#endif
}
