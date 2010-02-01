/*
 * contactlistutil.cpp
 * Copyright (C) 2009-2010  Michail Pishchagin
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

#include "contactlistutil.h"

#include <QModelIndex>
#include <QTextDocument> // for Qt::escape()

#include "psicontact.h"
#include "contactlistdragmodel.h"
#include "contactlistmodelselection.h"
#include "removeconfirmationmessagebox.h"
#include "contactlistdragview.h"

#ifdef YAPSI
#include "fakegroupcontact.h"
#endif

void ContactListUtil::removeContact(PsiContact* contact, QMimeData* _selection, ContactListDragModel* model, QWidget* widget, QObject* obj)
{
	Q_ASSERT(model);
	QModelIndexList indexes = model ? model->indexesFor(contact, _selection) : QModelIndexList();
	if (model && !indexes.isEmpty()) {
		QMimeData* selection = model->mimeData(indexes);
		QString selectionData = confirmationData(contact, _selection, model);

		// WARNING: selection could also contain groups. and when there are groups,
		// all groups' contacts are marked for deletion too
		QList<PsiContact*> contactsLost = model->contactsLostByRemove(selection);
		bool removeConfirmed = contactsLost.isEmpty();

		if (!removeConfirmed) {
			QStringList contactNames = contactNamesFor(contactsLost);

			QString destructiveActionName;
			QString msg;
			if (!contactNames.isEmpty()) {
				msg = tr("This will permanently remove<br>"
				         "%1"
				         "<br>from your contact list."
				        ).arg(contactNames.join(", "));
			}

#ifdef YAPSI
			QString complimentaryActionName;
			const char* complimentaryActionSlot = 0;

			ContactListModelSelection contactListSelection(selection);
			if (contactNames.count() > 1 || contactListSelection.groups().count()) {
				QStringList tmp;
				QStringList tmpContactNames = contactNames;
				while (!tmpContactNames.isEmpty()) {
					if (tmp.count() >= 10)
						break;

					tmp << QString("%1. %2").arg(tmp.count() + 1).arg(tmpContactNames.takeFirst());
				}

				QString andNContacts;
				if (!tmpContactNames.isEmpty()) {
					andNContacts = tr("and %n contacts ", 0,
					                  tmpContactNames.count());
				}

				if (contactListSelection.groups().count() > 1) {
					msg = tr("This will permanently remove:<br>"
					         "%1"
					         "<br>%2and %n groups from your contact list.",
					         0,
					         contactListSelection.groups().count()
					        ).arg(tmp.join("<br>"))
					         .arg(andNContacts);
				}
				else if (contactListSelection.groups().count() == 1) {
					msg = tr("This will permanently remove:<br>"
					         "%1"
					         "<br>%2and \"%3\" group from your contact list."
					        ).arg(tmp.join("<br>"))
					         .arg(andNContacts)
					         .arg(contactListSelection.groups().first().fullName);
				}
				else {
					msg = tr("This will permanently remove:<br>"
					         "%1"
					         "<br>%2from your contact list."
					        ).arg(tmp.join("<br>"))
					         .arg(andNContacts);
				}
			}

			if (indexes.count() == 1 && model->indexType(indexes.first()) == ContactListModel::GroupType) {
				if (YaContactListContactsModel::virtualUnremovableGroups().contains(contactListSelection.groups().first().fullName)) {
					msg = tr("This is a system group and can't be removed. "
					         "Permanently remove all its contacts from your contact list?");
					destructiveActionName = tr("Clear Group");
				}
				else {
					msg = tr("This will permanently remove<br>"
					         "%1"
					         "<br>group and all its contacts from your contact list.").arg(Qt::escape(indexes.first().data().toString()));
				}
			}
			else if (indexes.count() == 1 && model->indexType(indexes.first()) == ContactListModel::ContactType) {
				Q_ASSERT(contactListSelection.contacts().count() == 1);
				ContactListModelSelection::Contact c = contactListSelection.contacts().first();

				PsiAccount* account = contactList()->getAccount(c.account);
				PsiContact* psiContact = account ? account->findContact(c.jid) : 0;
				QStringList contactGroupsLostByRemove;
				if (psiContact) {
					contactGroupsLostByRemove = model->contactGroupsLostByRemove(psiContact, selection);
				}

				if (psiContact && !psiContact->inList() && psiContact->blockAvailable()) {
					msg = tr("This will permanently remove %1 from your contact list. "
					         "You could block it in order to avoid further messages.")
					      .arg(contactNames.join(", "));
					destructiveActionName = tr("Delete");
					complimentaryActionName = tr("Block");
					complimentaryActionSlot = "blockContactConfirmation";
				}
				else if (psiContact && psiContact->groups().count() > 1 && contactGroupsLostByRemove.count() == 1) {
					// TODO: needs to be translated
					msg = tr("This will remove %1 from \"%2\" group. "
					         "You could also remove it from all groups.")
					      .arg(contactNames.join(", "))
					      .arg(contactGroupsLostByRemove.first());
					destructiveActionName = tr("Delete");
					complimentaryActionName = tr("Delete From All Groups");
					// TODO: needs to be implemented
					complimentaryActionSlot = "removeContactFullyConfirmation";
				}
			}
#endif

			if (!msg.isEmpty()) {
#ifdef YAPSI
				if (complimentaryActionSlot) {
					RemoveConfirmationMessageBoxManager::instance()->
						removeConfirmation(selectionData,
						                   obj, "removeContactConfirmation",
						                   obj, complimentaryActionSlot,
						                   tr("Deleting contacts"),
						                   msg,
						                   widget,
						                   destructiveActionName,
						                   complimentaryActionName);
				}
#else
				if (false) {
				}
#endif
				else {
					RemoveConfirmationMessageBoxManager::instance()->
						removeConfirmation(selectionData,
						                   obj, "removeContactConfirmation",
						                   tr("Deleting contacts"),
						                   msg,
						                   widget,
						                   destructiveActionName);
				}

				removeConfirmed = false;
			}
		}

		QMetaObject::invokeMethod(obj, "removeContactConfirmation", Qt::DirectConnection,
		                          QGenericReturnArgument(),
		                          Q_ARG(QString, selectionData), Q_ARG(bool, removeConfirmed));
		delete selection;
	}
}

QString ContactListUtil::confirmationData(PsiContact* contact, QMimeData* _selection, ContactListDragModel* model)
{
	QString selectionData;
	QModelIndexList indexes = model->indexesFor(contact, _selection);
	if (model && !indexes.isEmpty()) {
		QMimeData* selection = model->mimeData(indexes);
		ContactListModelSelection* contactListModelSelection = dynamic_cast<ContactListModelSelection*>(selection);
		if (contactListModelSelection) {
			selectionData = QCA::Base64().arrayToString(contactListModelSelection->data(contactListModelSelection->mimeType()));
		}
	}
	return selectionData;
}

QList<PsiContact*> ContactListUtil::contactsFor(const QString& confirmationData, ContactListDragModel* model)
{
	Q_ASSERT(model);
	QList<PsiContact*> result;
	if (model) {
		ContactListModelSelection selection(0);
		selection.setData(selection.mimeType(), QCA::Base64().stringToArray(confirmationData).toByteArray());

		QModelIndexList indexes = model->indexesFor(0, &selection);
		foreach(const QModelIndex& index, indexes) {
			PsiContact* contact = model->contactFor(index);
			if (!contact)
				continue;

			result << contact;
		}
	}

	return result;
}

QStringList ContactListUtil::contactNamesFor(QList<PsiContact*> contacts)
{
	QStringList contactNames;
	foreach(PsiContact* contact, contacts) {
#ifdef YAPSI
		if (dynamic_cast<FakeGroupContact*>(contact)) {
			continue;
		}
#endif

		QString name = contact->name();
		if (name != contact->jid().full()) {
			// TODO: ideally it should be wrapped in <nobr></nobr>,
			// but it breaks layout in Qt 4.3.4
			name = tr("%1 (%2)").arg(name, Qt::escape(contact->jid().full()));
		}
		contactNames << QString("<b>%1</b>").arg(Qt::escape(name));
	}
	return contactNames;
}

void ContactListUtil::removeContactConfirmation(const QString& id, bool confirmed, ContactListDragModel* model, ContactListDragView* contactListView)
{
	Q_ASSERT(model);
	Q_ASSERT(contactListView);

	ContactListModelSelection selection(0);
	selection.setData(selection.mimeType(), QCA::Base64().stringToArray(id).toByteArray());

	if (confirmed) {
		removeContactConfirmation(&selection, model);
	}

	// message box steals focus, so we're restoring it back
	contactListView->restoreSelection(&selection);
	bringToFront(contactListView);
	contactListView->setFocus();
}

void ContactListUtil::removeContactConfirmation(QMimeData* contactSelection, ContactListDragModel* model)
{
	Q_ASSERT(contactSelection);
	Q_ASSERT(model);
	QModelIndexList indexes = model->indexesFor(0, contactSelection);
	if (model && !indexes.isEmpty()) {
		QMimeData* selection = model->mimeData(indexes);
		model->removeIndexes(selection);
		delete selection;
	}
}
