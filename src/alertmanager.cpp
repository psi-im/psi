/*
 * Copyright (C) 2008 Martin Hostettler
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Manager of a queue for alert popup dialogs (like can't connect to Server,
// connection errors, etc) the user has to attend to

#include <algorithm>
#include <QDebug>
#include <QMessageBox>

#include "alertmanager.h"


static bool itemCompare(AlertManager::Item* a, AlertManager::Item* b) {
	return a->priority > b->priority;
}

QWidget* AlertManager::findDialog(const QMetaObject& mo) const {
	foreach(Item* i, list_) {
		if (mo.cast(i->widget)) {
			return i->widget;
		}
	}
	return 0;
}

void AlertManager::findDialogs(const QMetaObject& mo, QList<void*>* list) const
{
	foreach(Item* i, list_) {
		if (mo.cast(i->widget)) {
			list->append(i->widget);
		}
	}
}

void AlertManager::dialogRegister(QWidget* w, int prio)
{
	qDebug() << "registered dialog";
	connect(w, SIGNAL(destroyed(QObject*)), SLOT(forceDialogUnregister(QObject*)));
	Item* i = new Item();
	i->widget = w;
	i->priority = prio;
	list_.append(i);
	std::push_heap(list_.begin(), list_.end(), itemCompare);

}

void AlertManager::dialogUnregister(QWidget* w)
{
	if (list_.at(0)->widget == w) {
		std::pop_heap(list_.begin(), list_.end(), itemCompare);
		list_.removeLast();
		if (!list_.isEmpty()) {
			list_.at(0)->widget->show();
		}
		return;
	}

	foreach(Item* i, list_) {
		if (i->widget == w) {
			list_.removeAll(i);
			delete i;
			break;
		}
	}
	std::make_heap(list_.begin(), list_.end(), itemCompare);

}

void AlertManager::forceDialogUnregister(QObject* obj)
{
	dialogUnregister(static_cast<QWidget*>(obj));
}


void AlertManager::deleteDialogList()
{
	while (!list_.isEmpty()) {
		Item* i = list_.takeLast();

		delete i->widget;

		delete i;
	}
}



bool AlertManager::raiseDialog(QWidget* w, int prio) {
	dialogRegister(w, prio);
	if (list_.at(0)->widget == w) {
		w->show();
		return true;
	}
	return false;
}

void AlertManager::raiseMessageBox(int prio, QMessageBox::Icon icon, const QString& title,
								   const QString& text)
{
	QMessageBox* msgBox = new QMessageBox(icon, title, text, QMessageBox::Ok, 0, Qt::WDestructiveClose);
	msgBox->setModal(false);
	
	raiseDialog(msgBox, prio);
}






AlertManager::AlertManager(PsiCon *psi) {
	psi_ = psi;
}


AlertManager::~AlertManager() {
	deleteDialogList();
}
