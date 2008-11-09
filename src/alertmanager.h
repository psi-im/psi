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

#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#include <QMessageBox>
#include "psicon.h"

class AlertManager : public QObject {
	Q_OBJECT
public:
	AlertManager(PsiCon *psi);
	~AlertManager();

	bool raiseDialog(QWidget* w, int prio);

	void raiseMessageBox(int prio, QMessageBox::Icon icon, const QString& title, const QString& text);

	void dialogRegister(QWidget* w, int prio);
	void dialogUnregister(QWidget* w);


	QWidget* findDialog(const QMetaObject& mo) const;
	template<typename T>
	inline T findDialog() const { 
		return static_cast<T>(findDialog(((T)0)->staticMetaObject));
	}

	void findDialogs(const QMetaObject& mo, QList<void*>* list) const;
	template<typename T>
	inline QList<T> findDialogs() const {
		QList<T> list;
		findDialogs(((T)0)->staticMetaObject,
		            reinterpret_cast<QList<void*>*>(&list));
		return list;
	}
	
	enum { AccountPassword = 100, ConnectionError=50 };
	
protected:
	void deleteDialogList();


private slots:
	void forceDialogUnregister(QObject* obj);

public:
	struct Item {
		QWidget* widget;
		int priority;
	};
	PsiCon *psi_;
	QList<Item*> list_;
};



#endif
