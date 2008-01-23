/*
 * bookmarksmanagerdlg.cpp - dialog for managing Bookmarks
 * stored in private storage
 * Copyright (C) 2006, 2008 Cestonaro Thilo, Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QObject>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

#include "mucjoindlg.h"
#include "conferencebookmark.h"
#include "urlbookmark.h"
#include "bookmarksmanagerdlg.h"
#include "groupchatdlg.h"

/**
 * \class Private, Dataholder for the BookmarksManagerDlg
 **/
class BookmarksManagerDlg::Private : public QObject
{
	Q_OBJECT
public:
	Private(BookmarksManagerDlg *parent) {
		parent_ = parent;
		pa = NULL;
		bm = NULL;
		topUrl = NULL;
		topConference = NULL;
		typeChange = false;
	}

	BookmarksManagerDlg *parent_;
	PsiAccount *pa;
	BookmarkManager *bm;
	QList<ConferenceBookmark> conferences;
	QList<URLBookmark> urls;
	QTreeWidgetItem *topUrl;
	QTreeWidgetItem *topConference;
	bool typeChange;
};

/**
 * \brief Construtor of the Dialog BookmarksManagerDlg
 * \param _pa, is the PsiAccount which this instance applies to
 **/
BookmarksManagerDlg::BookmarksManagerDlg(PsiAccount *pa)
:QDialog(0)
{
	/* qDebug("[BookmarksManagerDlg::BookmarksManagerDlg] - enter"); */
	d = new Private(this);
  	setupUi(this);
	setModal(false);
	d->pa = pa;
	getToplevelItems(&d->topUrl, &d->topConference);

	if(d->topUrl) {
		d->topUrl->setExpanded(true);
	}
	if(d->topConference) {
		d->topConference->setExpanded(true);
	}

	d->typeChange = false;

	d->bm = new BookmarkManager(d->pa->client());
	connect(d->bm, SIGNAL(getBookmarks_success(const QList<URLBookmark>&, const QList<ConferenceBookmark>&)),
			this, SLOT(getBookmarks_success(const QList<URLBookmark>&, const QList<ConferenceBookmark>&)));
	connect(d->bm, SIGNAL(getBookmarks_error(int, const QString&)),
			this, SLOT(getBookmarks_error(int, const QString&)));
	connect(d->bm, SIGNAL(setBookmarks_success()),
		this, SLOT(setBookmarks_success()));
	connect(d->bm, SIGNAL(setBookmarks_error(int, const QString&)),
		this, SLOT(setBookmarks_error(int, const QString&)));
	if(!d->pa->client()->isActive()) {
		QMessageBox::information(this, tr("Not connected"), tr("You are not connected to the server."));
	}
	else {
		d->bm->getBookmarks();
	}

	conferenceCheck->setChecked(true);
	urlCheck->setChecked(false);

	d->pa->dialogRegister(this, d->pa->jid());
	/* qDebug("[BookmarksManagerDlg::BookmarksManagerDlg] - leave"); */
}

/**
 * \brief Destructor of the Dialog BookmarksManagerDlg
 **/
BookmarksManagerDlg::~BookmarksManagerDlg()
{
	/* qDebug("[BookmarksManagerDlg::~BookmarksManagerDlg] - enter"); */
	d->pa->dialogUnregister(this);
	/* qDebug("[BookmarksManagerDlg::~BookmarksManagerDlg] - leave"); */
}

/**
 * \brief Slot onAccept, Button Ok of buttonBox pressed
 **/
void BookmarksManagerDlg::onAccept(void) {
	/* qDebug("[BookmarksManagerDlg::onAccept] - enter"); */
	QList<ConferenceBookmark> conferences_;
	QList<URLBookmark> urls_;
	QTreeWidgetItem *childItem;
	int index;
	d->typeChange = false;

	for( int childIndex = 0; childIndex < d->topUrl->childCount(); childIndex++) {
		childItem = d->topUrl->child(childIndex);
		index = childItem->data(0, ARRAYINDEX).toInt();
		if(index > -1 && d->urls.count() > index)
			urls_.append(d->urls[index]);
	}

	for( int childIndex = 0; childIndex < d->topConference->childCount(); childIndex++) {
		childItem = d->topConference->child(childIndex);
		index = childItem->data(0, ARRAYINDEX).toInt();
		if(index > -1 && d->conferences.count() > index)
			conferences_.append(d->conferences[index]);
	}

	/* qDebug("saving urls: %i, conferences: %i", urls_.count(), conferences_.count()); */
	d->bm->setBookmarks(urls_, conferences_);
	emit close();
	/* qDebug("[BookmarksManagerDlg::onAccept] - leave"); */
}

/**
 * \brief Slot onReject, Button Cancel of buttonBox pressed
 **/
void BookmarksManagerDlg::onReject(void) {
	/* qDebug("[BookmarksManagerDlg::onReject] - enter"); */
	d->typeChange = false;
	/* get the correct bookmarks, in case user changed something but want to recover them */
	d->bm->getBookmarks();
	emit close();
	/* qDebug("[BookmarksManagerDlg::onReject] - leave"); */
}

/**
 * \brief Slot onItemSelectionChanged, the user selected another item in the list of bookmarks
 **/
void BookmarksManagerDlg::onItemSelectionChanged() {
	/* qDebug("[BookmarksManagerDlg::onItemSelectionChanged] - enter"); */
	const QList<QTreeWidgetItem *> selectedItems = bookmarksList->selectedItems();
	int index;
	QTreeWidgetItem *currentItem;
	d->typeChange = false;

	if(selectedItems.count() > 0) {
		currentItem = selectedItems[0];
		index = currentItem->data(0, ARRAYINDEX).toInt();

		if(index > -1 && d->urls.count() > index && currentItem->parent() == d->topUrl) {
			conferenceCheck->setChecked(false);
			urlCheck->setChecked(true);
			URLBookmark url = d->urls[index];
			bookmarkName->setText(url.name());
			urlOrJid->setText(url.url());
			username->setText("");
			password->setText("");
			autoJoin->setChecked(false);
			username->setEnabled(false);
			password->setEnabled(false);
			autoJoin->setEnabled(false);
		} else if(index > -1 && d->conferences.count() > index && currentItem->parent() == d->topConference) {
			ConferenceBookmark c = d->conferences[index];
			
			conferenceCheck->setChecked(true);
			urlCheck->setChecked(false);
			
			bookmarkName->setText(c.name());
			urlOrJid->setText(c.jid().node() + "@" + c.jid().domain());
			username->setText(c.nick());
			password->setText(c.password());
			autoJoin->setChecked(c.autoJoin());
			username->setEnabled(true);
			password->setEnabled(true);
			autoJoin->setEnabled(true);
		} else {
			conferenceCheck->setChecked(true);
			urlCheck->setChecked(false);

			bookmarkName->setText("");
			urlOrJid->setText("");
			username->setText("");
			password->setText("");
			autoJoin->setChecked(false);
		}
	}
	/* qDebug("[BookmarksManagerDlg::onItemSelectionChanged] - leave"); */
}


/**
 * \brief Slot onItemDoubleClicked, an item of the TreeView was double clicked, now
 * 		  try to open the Url in a browser or the conference in a GCDlg
 **/
void BookmarksManagerDlg::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
	/* qDebug("[BookmarksManagerDlg::onItemDoubleClicked] - enter"); */
	Q_UNUSED(column);
	if(!item) {
		return;
	}

	int index = item->data(0, ARRAYINDEX).toInt();

	if(index > -1 && d->urls.count() > index && item->parent() == d->topUrl) {
		QDesktopServices::openUrl(QUrl(d->urls[index].url()));
	} else if(index > -1 && d->conferences.count() > index && item->parent() == d->topConference) {
		ConferenceBookmark c = d->conferences[index];
		if (!d->pa->findDialog<GCMainDlg*>(d->pa->jid())) {
			QString nick = c.nick();
			if (nick.isEmpty())
				nick = d->pa->jid().node();
			
			MUCJoinDlg *w = new MUCJoinDlg(d->pa->psi(), d->pa);
			w->le_host->setText(c.jid().domain());
			w->le_room->setText(c.jid().node());
			w->le_nick->setText(nick);
			w->le_pass->setText(c.password());
			w->show();
			w->doJoin();
		}
	}
	/* qDebug("[BookmarksManagerDlg::onItemDoubleClicked] - leave"); */
}

/**
 * Check the form for validity, and pop up warnings as appropriate.
 */
bool BookmarksManagerDlg::validateForm() {
    QString theName = bookmarkName->displayText();
	QString theUrlOrJid = urlOrJid->displayText();

	if(theUrlOrJid.count() < 1) {
		QMessageBox::warning(this, tr("Missing fields"), tr("Please specify the URL or conference to bookmark."));
		return false;
	}

	if(theName.count() < 1) {
		QMessageBox::warning(this, tr("Missing fields"), tr("Please specify a name for the bookmark."));
		return false;
	}
	
	if (!urlCheck->isChecked() && !conferenceCheck->isChecked()) {
		QMessageBox::warning(this, tr("Missing fields"), tr("Please specify the bookmark type."));
	}
	
    return true;
}

/**
 * \brief Slot onAdd, the Button btnAdd is pressed
 **/
void BookmarksManagerDlg::onAdd() {
	/* qDebug("[BookmarksManagerDlg::onAdd] - enter"); */
	QString theName;
	QString theUrlOrJid;
	int index;
	d->typeChange = false;

	theName = bookmarkName->displayText();
	theUrlOrJid = urlOrJid->displayText();
    
    if (!validateForm()) {
        return;
    }
    
	if (urlCheck->isChecked()) {
		/* qDebug("[BookmarksManagerDlg::onAdd] - adding a Url"); */
		URLBookmark url(theName, theUrlOrJid);
		index = d->urls.count();
		d->urls.append(url);
		if(d->topUrl) {
			addItem(d->topUrl, index, url.name(), url.url());
		}
    } else if (conferenceCheck->isChecked()) {
		/* qDebug("[BookmarksManagerDlg::onAdd] - adding a Conference"); */
		ConferenceBookmark conference(theName, theUrlOrJid, autoJoin->isChecked(), username->displayText(), password->displayText());
		index = d->conferences.count();
		d->conferences.append(conference);
		if (d->topConference) {
			addItem(d->topConference, index, conference.name(), conference.jid().node() + "@" + conference.jid().domain());
		}
	} 

	/* qDebug("[BookmarksManagerDlg::onAdd] - leave"); */
}

/**
 * \brief Slot onModify, the Button btnModify is pressed
 **/
void BookmarksManagerDlg::onModify() {
	/* qDebug("[BookmarksManagerDlg::onModify] - enter"); */
	const QList<QTreeWidgetItem *> selectedItems = bookmarksList->selectedItems();
	QString theName;
	QString theUrlOrJid;
	int index = 0;
	QTreeWidgetItem *current = NULL;

	if(selectedItems.count() > 0) {
		current = selectedItems[0];
		index = current->data(0, ARRAYINDEX).toInt();

		if (!validateForm()) {
            return;
		}
		if (!d->typeChange) {
			if(index > -1 && d->urls.count() > index && urlCheck->isChecked()) {
				/* qDebug("[BookmarksManagerDlg::onModify] - modifying a Url at Index: %i", index); */
				URLBookmark url(theName, theUrlOrJid);
				d->urls.replace(index, url);
			} else if(index > -1 && d->conferences.count() > index && conferenceCheck->isChecked()) {
				/* qDebug("[BookmarksManagerDlg::onModify] - modifying a Conference at Index: %i", index); */
				ConferenceBookmark conference(theName, theUrlOrJid, autoJoin->isChecked(), username->displayText(), password->displayText());
				d->conferences.replace(index, conference);
			}
			current->setText(0, theName);
			current->setText(1, theUrlOrJid);
		} else {
			/* qDebug("[BookmarksManagerDlg::onModify] - calling onAdd()"); */
			onAdd();
			/* qDebug("[BookmarksManagerDlg::onModify] - after onAdd()"); */
			if (index > -1 && d->conferences.count() > index && urlCheck->isChecked()) {
				/* qDebug("[BookmarksManagerDlg::onModify] - modifying a Url at Index: %i", index); */
				d->conferences.removeAt(index);
			} else if (index > -1 && d->urls.count() > index && conferenceCheck->isChecked()) {
				/* qDebug("[BookmarksManagerDlg::onModify] - modifying a Conference at Index: %i", index); */
				d->urls.removeAt(index);
			}
			current->parent()->takeChild(current->parent()->indexOfChild(current));
			d->typeChange = false;
		}
	}
	/* qDebug("[BookmarksManagerDlg::onModify] - leave"); */
}

/**
 * \brief Slot onRemove, the Button btnRemove is pressed
 **/
void BookmarksManagerDlg::onRemove(void) {
	/* qDebug("[BookmarksManagerDlg::onRemove] - enter"); */
	const QList<QTreeWidgetItem *> selectedItems = bookmarksList->selectedItems();
	int index = -1;
	unsigned int answer = QMessageBox::NoButton;
	QTreeWidgetItem *current = NULL;
	d->typeChange = false;

	if (selectedItems.count() > 0) { 
		current = selectedItems[0];
		index = current->data(0, ARRAYINDEX).toInt();

		if (index > -1 && d->urls.count() > index && current->parent() == d->topUrl) {
			URLBookmark url = d->urls[index];
			answer = QMessageBox::question(this, tr("Confirm deletion"),
					QString(tr("Do you really want to delete the URL Bookmark '%1'?")).arg(url.name()),
				   	QMessageBox::Yes | QMessageBox::No);
		} else if (index > -1 && d->conferences.count() > index && current->parent() == d->topConference) {
			ConferenceBookmark conference = d->conferences[index];
			answer = QMessageBox::question(this, tr("Confirm deletion"),
					QString(tr("Do you really want to delete the conference Bookmark '%1'?")).arg(conference.name()),
				   	QMessageBox::Yes | QMessageBox::No);
		}

		if (answer == QMessageBox::Yes) {
			/* later only save the list items, so it's enough to delete from list
			 else you need to the all indexes in the every item of the list
			 which's Index has changed */
			current->parent()->takeChild(current->parent()->indexOfChild(current));
		}
	}
	/* qDebug("[BookmarksManagerDlg::onRemove] - leave"); */
}

/**
 * \brief Slot getBookmarks_success, this slot is called when the XMPP task for getting the Bookmarks 
 *		  succeeded. So we can create the listitems and so on.
 **/
void BookmarksManagerDlg::getBookmarks_success(const QList<URLBookmark>& _urls, const QList<ConferenceBookmark>& _conferences) {
	/* qDebug("[BookmarksManagerDlg::getBookmarks_success] - enter"); */
	int index;

	d->urls = _urls;
	d->conferences = _conferences;
	
	index = 0;
	if (d->topUrl && d->urls.count() > 0) {
		/* qDebug("[BookmarksManagerDlg::getBookmarks_success] - add Urls to TreeWidget."); */
		d->topUrl->takeChildren();
		foreach (URLBookmark u, d->urls) {
			addItem(d->topUrl, index, u.name(), u.url());
			index++;
		}
		index = 0;
	}

	if (d->topConference && d->conferences.count() > 0) {
		/* qDebug("[BookmarksManagerDlg::getBookmarks_success] - add Conferences to TreeWidget."); */
		d->topConference->takeChildren();
		foreach (ConferenceBookmark c, d->conferences) {
			addItem(d->topConference, index, c.name(), c.jid().node() + "@" + c.jid().domain());
			index++;
		}	
	}
	/* qDebug("[BookmarksManagerDlg::getBookmarks_success] - leave"); */
}

/**
 * \brief Slot getBookmarks_error, this slot is called when the XMPP task for getting the Bookmarks 
 *		  failed. Currently does nothing.
 * \param i?!
 * \param s?!
 **/
void BookmarksManagerDlg::getBookmarks_error(int i, const QString& s) {
	/* qDebug("[BookmarksManagerDlg::getBookmarks_error] - enter"); */
	Q_UNUSED(i); Q_UNUSED(s); 
	//FIXME shouldn't ignore error stotes.
	qDebug("[BookmarksManagerDlg::getBookmarks_error] - leave");
}

/**
 * \brief getToplevelItems, this function retrieves and sets the two toplevel Items. One for the Toplevel item
 * 		  of the Urls and one for the Toplevel item of the conferences.
 * \param topUrl, a pointer to the pointer which should retrieve the pointer to the toplevel item for the urls
 * \param topConference, a pointer to the pointer which should retrieve the pointer to the toplevel item for the conferences
 **/
void BookmarksManagerDlg::getToplevelItems(QTreeWidgetItem **topUrl, QTreeWidgetItem **topConference) {
	/* qDebug("[BookmarksManagerDlg::getToplevelItems] - enter"); */
	if (bookmarksList->topLevelItemCount() == 2) {
		if (topUrl) *topUrl = bookmarksList->topLevelItem(0);
		if (topConference) *topConference = bookmarksList->topLevelItem(1);
	} else {
		if (topUrl) *topUrl = NULL;
		if (topConference) *topConference = NULL;
	}
	/* qDebug("[BookmarksManagerDlg::getToplevelItems] - leave"); */
}

/**
 * \brief addItem, this function creates a new TreeView item and appends it to the parent
 * \param parent, the Parent QTreeWidgetItem, which gets the child
 * \param index, this is the index of the corresponding QList<XXXX> item, which will be set as ARRAYINDEX, a custom Role, to the
 * 		  first column of the item.
 * \param sName, the text which will be shown in the first column of the item.
 * \param sUrlOrJid, the text which will be shown in the second column of the item.
 **/
void BookmarksManagerDlg::addItem(QTreeWidgetItem *parent, int index, QString name, QString urlOrJid) {
	/* qDebug("[BookmarksManagerDlg::addItem] - enter"); */
	if (parent) {
		QTreeWidgetItem *child = new QTreeWidgetItem(parent);
		if (child) {
			child->setText(0, name);
			child->setData(0, ARRAYINDEX, QVariant(index));
			child->setText(1, urlOrJid);
			parent->addChild(child);
			parent->sortChildren(0, Qt::AscendingOrder);
		}
	}
	/* qDebug("[BookmarksManagerDlg::addItem] - leave"); */
}

/**
 * \brief Slot setBookmarks_success, called when the bookmarks where successfully set.
 **/
void BookmarksManagerDlg::setBookmarks_success() {
	/* qDebug("[BookmarksManagerDlg::setBookmarks_success] - enter"); */
	/* qDebug("[BookmarksManagerDlg::setBookmarks_success] - leave"); */
}

/**
 * \brief Slot setBookmarks_error, called when the bookmarks could not be successfully set.
 * \param i?!
 * \param s?!
 **/
void BookmarksManagerDlg::setBookmarks_error(int i, const QString& s) {
	/* qDebug("[BookmarksManagerDlg::setBookmarks_error] - enter"); */
	Q_UNUSED(i); Q_UNUSED(s);
	QMessageBox::critical(this, tr("Save not successful"), tr("The bookmarks could not be saved successfully on the server."));
	/* qDebug("[BookmarksManagerDlg::setBookmarks_error] - leave"); */
}

/**
 * \brief Slot onRBClicked, called when one of the two RadioButtons (urlCheck, conferenceCheck) is pressed.
 **/
void BookmarksManagerDlg::onRBClicked() {
	/* qDebug("[BookmarksManagerDlg::onRBClicked] - enter"); */
	if (urlCheck->isChecked()) {
		username->setEnabled(false);
		password->setEnabled(false);
		autoJoin->setEnabled(false);
	} else {
		username->setEnabled(true);
		password->setEnabled(true);
		autoJoin->setEnabled(true);
	}
	d->typeChange = !d->typeChange;
	/* qDebug("[BookmarksManagerDlg::onRBClicked] - leave"); */
}

#include "bookmarksmanagerdlg.moc"
