#ifndef CONTACTLISTVIEW_H
#define CONTACTLISTVIEW_H

#include <QTreeView>

class QWidget;

class ContactListView : public QTreeView
{
	Q_OBJECT

public:
	ContactListView(QWidget* parent = 0);

	bool largeIcons() const;
	bool showIcons() const;

	// Reimplemented
	void setModel(QAbstractItemModel* model);

public slots:
	void resetExpandedState();
	void setShowIcons(bool);
	void setLargeIcons(bool);
	void resizeColumns();

protected:
	void contextMenuEvent(QContextMenuEvent*);
	virtual void doItemsLayout();
	//void drawBranches(QPainter*, const QRect&, const QModelIndex&) const;

private:
	bool largeIcons_, showIcons_;
};

#endif
