#include <QTreeView>
#include <QHeaderView>
#include <QContextMenuEvent>

#include "contactlistmodel.h"
#include "contactlistview.h"

ContactListView::ContactListView(QWidget* parent) : QTreeView(parent)
{
	setUniformRowHeights(false);
	//v.setIndentation(0);
	setAlternatingRowColors(true);
	setLargeIcons(false);
	setShowIcons(true);
	setRootIsDecorated(false);
	setEditTriggers(QAbstractItemView::EditKeyPressed|QAbstractItemView::AnyKeyPressed);
	setIndentation(5);
	header()->hide();
	header()->setStretchLastSection(false);
//#if QT_VERSION >= 0x040100
//	for (int i = 0; i < headerView->count(); i++) {
//		headerView->setResizeMode(i,QHeaderView::Interactive);
//	}
//	headerView->setResizeMode(1,QHeaderView::Stretch);
//#else
//	moveSection(ContactListModel::PictureColumn, ContactListModel::TextColumn);
//#endif

	//connect(this,SIGNAL(activated(const QModelIndex&)),SLOT(item_activated(const QModelIndex&)));
}

bool ContactListView::showIcons() const
{
	return showIcons_;
}

bool ContactListView::largeIcons() const
{
	return largeIcons_;
}

void ContactListView::setLargeIcons(bool b)
{
	largeIcons_ = b;
	setIconSize(largeIcons_ ? QSize(32,32) : QSize(16,16));
	resizeColumns();
}

void ContactListView::setShowIcons(bool b)
{
	showIcons_ = b;
	if (header()->count() > ContactListModel::PictureColumn) {
		header()->setSectionHidden(ContactListModel::PictureColumn,!b);
	}
}


void ContactListView::resetExpandedState()
{
	QAbstractItemModel* m = model();
	for (int i=0; i < m->rowCount(QModelIndex()); i++) {
		QModelIndex index = m->index(i,0);
		setExpanded(index,m->data(index,ContactListModel::ExpandedRole).toBool());
	}
}

void ContactListView::doItemsLayout()
{
	QTreeView::doItemsLayout();
	resetExpandedState();
}

void ContactListView::contextMenuEvent(QContextMenuEvent* e)
{
	model()->setData(indexAt(e->pos()),QVariant(mapToGlobal(e->pos())),ContactListModel::ContextMenuRole);
}

void ContactListView::resizeColumns()
{
	QHeaderView* headerView = header();
	int columns = headerView->count();
	if (columns > ContactListModel::NameColumn) {
		headerView->setResizeMode(ContactListModel::NameColumn,QHeaderView::Stretch);
	}
	if (columns > ContactListModel::StatusIconColumn) {
		headerView->setResizeMode(ContactListModel::StatusIconColumn,QHeaderView::Custom);
		headerView->resizeSection(ContactListModel::StatusIconColumn,iconSize().width());
	}
	if (columns >  ContactListModel::PictureColumn) {
		headerView->setResizeMode(ContactListModel::PictureColumn,QHeaderView::Custom);
		headerView->resizeSection(ContactListModel::PictureColumn,iconSize().width());
	}
}

void ContactListView::setModel(QAbstractItemModel* model)
{
	QTreeView::setModel(model);
	resizeColumns();
	setShowIcons(showIcons());
}

// Branches ? We don't want no steenking branches !
/*void ContactListView::drawBranches(QPainter*, const QRect&, const QModelIndex&) const
{
}*/
