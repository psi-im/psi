/*
 * removeconfirmationmessagebox.h - generic confirmation of destructive action
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef REMOVECONFIRMATIONMESSAGEBOX_H

#include <QMessageBox>
#include <QPointer>

class QPushButton;

class RemoveConfirmationMessageBoxManager : public QObject
{
	Q_OBJECT
public:
	static RemoveConfirmationMessageBoxManager* instance();

	void showInformation(const QString& id,
	                     const QString& title, const QString& informativeText,
	                     QWidget* parent);

	// slots must accept (QString id, bool confirmed)
	void removeConfirmation(const QString& id, QObject* obj, const char* slot,
	                        const QString& title, const QString& informativeText,
	                        QWidget* parent, const QString& destructiveActionName = QString());

	// slots must accept (QString id, bool confirmed)
	void removeConfirmation(const QString& id,
	                        QObject* obj1, const char* action1slot,
	                        QObject* obj2, const char* action2slot,
	                        const QString& title, const QString& informativeText,
	                        QWidget* parent,
	                        const QString& action1name,
	                        const QString& action2name);

private slots:
#ifdef YAPSI_ACTIVEX_SERVER
	void onlineCallback(const QString& id, int button);
#endif
	void update();

private:
	RemoveConfirmationMessageBoxManager();
	~RemoveConfirmationMessageBoxManager();

	struct DataCallback {
		DataCallback(QObject* _obj, const char* _slot)
			: obj(_obj), slot(_slot)
		{
			Q_ASSERT(!obj.isNull());
			Q_ASSERT(slot);
		}

		QPointer<QObject> obj;
		const char* slot;
	};

	struct Data {
		QString title;
		QString informativeText;
		QStringList buttons;
		QWidget* parent;
		QMessageBox::Icon icon;

		int onlineId;
		QString id;
		QList<DataCallback> callbacks;
	};

	void processData(const QString& id,
	                 const QList<DataCallback> callbacks,
	                 const QString& title, const QString& informativeText,
	                 QWidget* parent,
	                 const QStringList& actionNames,
	                 QMessageBox::Icon icon);

	static RemoveConfirmationMessageBoxManager* instance_;
	QList<Data> data_;
	static int onlineId_;
};

class RemoveConfirmationMessageBox : public QMessageBox
{
	Q_OBJECT
protected:
	RemoveConfirmationMessageBox(const QString& title, const QString& informativeText, QWidget* parent);

	void setDestructiveActionName(const QString& destructiveAction);
	void setComplimentaryActionName(const QString& complimentaryAction);
	void setInfoActionName(const QString& infoAction);

	void doExec();

	bool removeAction() const;
	bool complimentaryAction() const;
	bool infoAction() const;

	static QString processInformativeText(const QString& informativeText);

private:
	QPushButton* removeButton_;
	QPushButton* complimentaryButton_;
	QPushButton* infoButton_;
	QPushButton* cancelButton_;

	friend class RemoveConfirmationMessageBoxManager;
};

#endif
