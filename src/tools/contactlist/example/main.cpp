#include <QApplication>
#include <QTreeView>
#include <QMainWindow>
#include <QDirModel>
#include <QIcon>
#include <QHeaderView>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>

#include "status.h"
#include "contactlistmodel.h"
#include "contactlistview.h"

#include "mycontact.h"
#include "mycontactlist.h"

void addRandom(MyContactList* c, int n, int perGroup)
{
	for (int i = 0; i < n; i++) {
		Status::Type t = (Status::Type) (i % 6);
		c->addContact(QString("Contact %1").arg(i),QString("contact%1@foo.com").arg(i),"",Status(t,""),QString("Group %1").arg(i / perGroup));
	}
}

class MyContactListWidget : public QMainWindow
{
	Q_OBJECT

public:
	MyContactListWidget();

public slots:
	void setIndent(bool);
	void resetSearch();
	void search(const QString&);
	void setAlternatingRowColors(bool b);
	void setRootIsDecorated(bool b);

private:
	QLineEdit* search_text_;
	ContactListView* view_;
	MyContactList* contactlist_;
	bool indent_;
	int default_indent_;
};

MyContactListWidget::MyContactListWidget()
{
	QWidget* w = new QWidget(this);
	setCentralWidget(w);
	setAttribute(Qt::WA_MacMetalStyle);

	/* Set up contact list */
	contactlist_ = new MyContactList();
	contactlist_->addContact(
		"Peter Gibbons", "peter@initech.com", ":/pictures/peter.jpg",
		Status(Status::Online,"Here"));
	contactlist_->addContact(
		"Tom Smykowski", "tom@initech.com", "",
		Status(Status::Online,""),"Co-workers");
	contactlist_->addContact(
		"Mike Bolton", "mike@initech.com", ":/pictures/mike.jpg",
		Status(Status::Online,""),"Co-workers");
	contactlist_->addContact(
		"Samir Nagheenanajar", "samir@initech.com", ":/pictures/samir.jpg",
		Status(Status::XA,"At Chotchkie's"),"Co-workers");
	contactlist_->addContact(
		"Milton Waddams", "milton@initech.com", ":/pictures/milton.jpg",
		Status(Status::Away,"Collating"),"Co-workers");
	contactlist_->addContact(
		"Bill Lumbergh", "bill@initech.com", ":/pictures/bill.jpg",
		Status(Status::DND,""),"Co-workers");
	contactlist_->addContact(
		"Bob Slydell", "bob1@initech.com", "",
		Status(Status::Offline,""),"Co-workers");
	contactlist_->addContact(
		"Bob Porter", "bob2@initech.com", "",
		Status(Status::Offline,""),"Co-workers");
	contactlist_->addContact(
		"Joanna", "joanna@chotchkies.com", ":/pictures/joanna.jpg",
		Status(Status::FFC,""),"Friends");
	contactlist_->addContact(
		"Lawrence", "lawrence@construction.com", ":/pictures/lawrence.jpg",
		Status(Status::Offline,""),"Friends");
	contactlist_->addContact("Brian","brian@chotchkies.com",":/pictures/brian.jpg", Status(Status::Offline,""),"Others");
	contactlist_->addContact("Anne","anne@amihotornot.com",":/pictures/anne.jpg", Status(Status::Offline,""),"Others");

	// Random
	/*addRandom(contactlist_,200,20);
	addRandom(contactlist_,200,10);
	addRandom(contactlist_,200,400);*/

	/* Set up model & view */
	ContactListModel *model = new ContactListModel(contactlist_);
	view_ = new ContactListView(centralWidget());
	view_->setModel(model);

	/* Initialize variables */
	default_indent_ = view_->indentation();
	indent_ = true;

	/* Create vertical layout */
	QVBoxLayout* layout = new QVBoxLayout(centralWidget());
	layout->addWidget(view_);

	/* Create search bar */
	QWidget* search_widget = new QWidget(centralWidget());
	layout->addWidget(search_widget);
	QHBoxLayout* search_layout = new QHBoxLayout(search_widget);
	search_text_ = new QLineEdit(search_widget);
	connect(search_text_,SIGNAL(textChanged(const QString&)),SLOT(search(const QString&)));
	search_layout->addWidget(search_text_);
	QToolButton* search_pb = new QToolButton(search_widget);
	search_pb->setIcon(QIcon(":/close.png"));
	connect(search_pb,SIGNAL(clicked()),SLOT(resetSearch()));
	search_layout->addWidget(search_pb);

	/* Set up menu */
	//new QMenuBar(this);
	QMenu* viewMenu = menuBar()->addMenu("View");

	QAction* showIcons = new QAction("Show Icons",this);
	showIcons->setCheckable(true);
	showIcons->setChecked(view_->showIcons());
	QObject::connect(showIcons,SIGNAL(toggled(bool)),view_,SLOT(setShowIcons(bool)));
	viewMenu->addAction(showIcons);

	QAction* largeIcons = new QAction("Large Icons",this);
	largeIcons->setCheckable(true);
	largeIcons->setChecked(view_->largeIcons());
	QObject::connect(largeIcons,SIGNAL(toggled(bool)),view_,SLOT(setLargeIcons(bool)));
	viewMenu->addAction(largeIcons);

	QAction* indentation = new QAction("Indentation",this);
	indentation->setCheckable(true);
	indentation->setChecked(indent_);
	QObject::connect(indentation,SIGNAL(toggled(bool)),SLOT(setIndent(bool)));
	viewMenu->addAction(indentation);

	QAction* alternatingColors = new QAction("Alternating Row Colors",this);
	alternatingColors->setCheckable(true);
	alternatingColors->setChecked(view_->alternatingRowColors());
	QObject::connect(alternatingColors,SIGNAL(toggled(bool)),this,SLOT(setAlternatingRowColors(bool)));
	viewMenu->addAction(alternatingColors);

	QAction* decorateRoot = new QAction("Decorate Root",this);
	decorateRoot->setCheckable(true);
	decorateRoot->setChecked(view_->rootIsDecorated());
	QObject::connect(decorateRoot,SIGNAL(toggled(bool)),this,SLOT(setRootIsDecorated(bool)));
	viewMenu->addAction(decorateRoot);


	viewMenu->addSeparator();

	QAction* showOffline = new QAction("Show Offline Contacts",this);
	showOffline->setCheckable(true);
	showOffline->setChecked(contactlist_->showOffline());
	QObject::connect(showOffline,SIGNAL(toggled(bool)),contactlist_,SLOT(setShowOffline(bool)));
	viewMenu->addAction(showOffline);

	QAction* showGroups = new QAction("Show Groups",this);
	showGroups->setCheckable(true);
	showGroups->setChecked(contactlist_->showGroups());
	QObject::connect(showGroups,SIGNAL(toggled(bool)),contactlist_,SLOT(setShowGroups(bool)));
	viewMenu->addAction(showGroups);

	/* Set up title */
	setWindowTitle("Contact List");
}

void MyContactListWidget::resetSearch()
{
	search_text_->clear();
}

void MyContactListWidget::setIndent(bool b)
{
	indent_ = b;
	view_->setIndentation(indent_ ? default_indent_ : 0);
	view_->reset();
}

void MyContactListWidget::setAlternatingRowColors(bool b)
{
	view_->setAlternatingRowColors(b);
}

void MyContactListWidget::setRootIsDecorated(bool b)
{
	view_->setRootIsDecorated(b);
}

void MyContactListWidget::search(const QString& search)
{
	contactlist_->setSearch(search);
}


int main(int argc, char* argv[])
{
	QApplication app(argc,argv);
	MyContactListWidget w;
	w.setGeometry(100,100,230,500);
	w.show();
	return app.exec();
}

#include "main.moc"
