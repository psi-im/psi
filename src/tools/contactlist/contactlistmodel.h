#ifndef CONTACTLISTMODEL_H
#define CONTACTLISTMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

class ContactList;

class ContactListModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum {
		ExpandedRole = Qt::UserRole + 0,
		ContextMenuRole = Qt::UserRole + 1
	};
	enum {
		StatusIconColumn = 0,
		NameColumn = 1,
		PictureColumn = 2
	};

	ContactListModel(ContactList* contactList);

	// Reimplemented from QAbstratItemModel
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
	virtual QModelIndex parent(const QModelIndex &index) const;
	virtual int rowCount(const QModelIndex &parent) const;
	virtual int columnCount(const QModelIndex &parent) const;
	Qt::ItemFlags flags(const QModelIndex& index) const;
	virtual bool setData(const QModelIndex&, const QVariant&, int role);

protected slots:
	void contactList_changed();

private:
	ContactList* contactList_;
	bool showStatus_;
};

#endif
