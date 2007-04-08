#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include <QObject>

class ContactListGroupItem;
class ContactListRootItem;
class ContactListItemComparator;

class ContactList : public QObject
{
	Q_OBJECT

	friend class ContactListModel;

public:
	ContactList(QObject* parent = 0);
	bool showOffline() const { return showOffline_; }
	bool showGroups() const { return showGroups_; }
	ContactListRootItem* invisibleGroup();
	ContactListRootItem* rootItem();
	const ContactListItemComparator* itemComparator() const;
	const QString& search() const;

signals:
	void dataChanged();

public slots:
	void setShowOffline(bool);
	void setShowGroups(bool);
	void setSearch(const QString&);
	void emitDataChanged(); // Not wild about this one

protected:
	void updateParents();
	void updateVisibleParents();
	void updateInvisibleParents();

	//ContactListGroupItem* hiddenGroup();
	//ContactListGroupItem* agentsGroup();
	//ContactListGroupItem* conferenceGroup();

private:
	bool showOffline_, showGroups_;
	QString search_;
	ContactListItemComparator* itemComparator_;

	ContactListRootItem* rootItem_;
	ContactListRootItem* invisibleGroup_;
	ContactListRootItem* altInvisibleGroup_;

	//ContactListGroupItem* hiddenGroup_;
	//ContactListGroupItem* agentsGroup_;
	//ContactListGroupItem* conferenceGroup_;
};

#endif
