/*
 * contactlistutil.h
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

#ifndef CONTACTLISTUTIL_H
#define CONTACTLISTUTIL_H

#include <QObject>

class PsiContact;
class QMimeData;
class ContactListDragModel;
class ContactListDragView;

class ContactListUtil : public QObject
{
	Q_OBJECT
public:
	static void removeContact(PsiContact* contact, QMimeData* _selection, ContactListDragModel* model, QWidget* widget, QObject* obj);
	static QString confirmationData(PsiContact* contact, QMimeData* _selection, ContactListDragModel* model);

	static void removeContactConfirmation(const QString& id, bool confirmed, ContactListDragModel* model, ContactListDragView* contactListView);
	static void removeContactConfirmation(QMimeData* contactSelection, ContactListDragModel* model);

	static QList<PsiContact*> contactsFor(const QString& confirmationData, ContactListDragModel* model);
	static QStringList contactNamesFor(QList<PsiContact*> contacts);

private:
	ContactListUtil() {}
};

#endif
