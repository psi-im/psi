/*
 * removeconfirmationmessagebox.cpp - generic confirmation of destructive action
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

#include "removeconfirmationmessagebox.h"

#include <QApplication>
#include <QPushButton>
#include <QTimer>
#include <QStyle>

#include "applicationinfo.h"

//----------------------------------------------------------------------------
// RemoveConfirmationMessageBoxManager
//----------------------------------------------------------------------------

RemoveConfirmationMessageBoxManager* RemoveConfirmationMessageBoxManager::instance_ = 0;
int RemoveConfirmationMessageBoxManager::onlineId_ = 0;

RemoveConfirmationMessageBoxManager* RemoveConfirmationMessageBoxManager::instance()
{
	if (!instance_) {
		instance_ = new RemoveConfirmationMessageBoxManager();
	}
	return instance_;
}

void RemoveConfirmationMessageBoxManager::processData(const QString& id, const QList<DataCallback> callbacks, const QString& title, const QString& informativeText, QWidget* parent, const QStringList& actionNames, QMessageBox::Icon icon)
{
	Data data;
	data.onlineId = ++onlineId_;
	data.id = id;
	data.callbacks = callbacks;
	data.title = title;
	data.informativeText = informativeText;
	data.buttons = actionNames;
	data.parent = parent;
	data.icon = icon;

	// FIXME: duplicates mustn't be allowed
	foreach(Data d, data_) {
		Q_ASSERT(d.id != id);
		if (d.id == id)
			return;
	}

	data_ << data;
	QTimer::singleShot(0, this, SLOT(update()));
}

void RemoveConfirmationMessageBoxManager::showInformation(const QString& id, const QString& title, const QString& informativeText, QWidget* parent)
{
	QStringList buttons;
	buttons << RemoveConfirmationMessageBox::tr("OK");

	QList<DataCallback> callbacks;

	processData(id, callbacks, title, informativeText, parent, buttons, QMessageBox::Information);
}

void RemoveConfirmationMessageBoxManager::removeConfirmation(const QString& id, QObject* obj, const char* slot, const QString& title, const QString& informativeText, QWidget* parent, const QString& destructiveActionName)
{
	QStringList buttons;
	if (!destructiveActionName.isEmpty())
		buttons << destructiveActionName;
	else
		buttons << RemoveConfirmationMessageBox::tr("Delete");
	buttons << RemoveConfirmationMessageBox::tr("Cancel");

	QList<DataCallback> callbacks;
	callbacks << DataCallback(obj, slot);

	processData(id, callbacks, title, informativeText, parent, buttons, QMessageBox::Warning);
}

void RemoveConfirmationMessageBoxManager::removeConfirmation(const QString& id, QObject* obj1, const char* action1slot, QObject* obj2, const char* action2slot, const QString& title, const QString& informativeText, QWidget* parent, const QString& action1name, const QString& action2name)
{
	QStringList buttons;
	buttons << action1name;
	buttons << action2name;
	buttons << RemoveConfirmationMessageBox::tr("Cancel");

	QList<DataCallback> callbacks;
	callbacks << DataCallback(obj1, action1slot);
	callbacks << DataCallback(obj2, action2slot);

	processData(id, callbacks, title, informativeText, parent, buttons, QMessageBox::Warning);
}

void RemoveConfirmationMessageBoxManager::update()
{
	while (!data_.isEmpty()) {
		Data data = data_.takeFirst();

		Q_ASSERT(data.buttons.count() >= 1 && data.buttons.count() <= 3);
		RemoveConfirmationMessageBox msgBox(data.title, data.informativeText, data.parent);
		msgBox.setIcon(data.icon);

		QStringList buttons = data.buttons;
		if (data.icon == QMessageBox::Warning) {
			buttons.takeLast(); // Cancel
			Q_ASSERT(!buttons.isEmpty());
			msgBox.setDestructiveActionName(buttons.takeFirst());
			if (!buttons.isEmpty()) {
				msgBox.setComplimentaryActionName(buttons.takeFirst());
			}
		}
		else {
			msgBox.setInfoActionName(buttons.takeFirst());
		}

		msgBox.doExec();

		QList<bool> callbackData;
		for (int i = 0; i < data.callbacks.count(); ++i) {
			if (i == 0)
				callbackData << msgBox.removeAction();
			else if (i == 1)
				callbackData << msgBox.complimentaryAction();
			else
				callbackData << false;
		}

		for (int i = 0; i < data.callbacks.count(); ++i) {
			QMetaObject::invokeMethod(data.callbacks[i].obj, data.callbacks[i].slot, Qt::DirectConnection,
			                          QGenericReturnArgument(),
			                          Q_ARG(QString, data.id),
			                          Q_ARG(bool, callbackData[i]));
		}
	}
}

RemoveConfirmationMessageBoxManager::RemoveConfirmationMessageBoxManager()
	: QObject(QCoreApplication::instance())
{
}

RemoveConfirmationMessageBoxManager::~RemoveConfirmationMessageBoxManager()
{
}

//----------------------------------------------------------------------------
// RemoveConfirmationMessageBox
//----------------------------------------------------------------------------
RemoveConfirmationMessageBox::RemoveConfirmationMessageBox(const QString& title, const QString& informativeText, QWidget* parent)
	: QMessageBox()
	, removeButton_(0)
	, complimentaryButton_(0)
	, cancelButton_(0)
	, infoButton_(0)
{
	setWindowTitle(ApplicationInfo::name());

	setText(title);
	setInformativeText(informativeText);

	setIcon(QMessageBox::Warning);
	int iconSize = style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
	QIcon tmpIcon= style()->standardIcon(QStyle::SP_MessageBoxWarning);
	if (!tmpIcon.isNull())
		setIconPixmap(tmpIcon.pixmap(iconSize, iconSize));

	// doesn't work with borderless top-level windows on Mac OS X
	// QWidget* window = parent->window();
	// msgBox.setParent(window);
	// msgBox.setWindowFlags(Qt::Sheet);
	Q_UNUSED(parent);
}

void RemoveConfirmationMessageBox::setDestructiveActionName(const QString& destructiveAction)
{
	Q_ASSERT(!removeButton_);
	Q_ASSERT(!cancelButton_);
	Q_ASSERT(!infoButton_);
	removeButton_ = addButton(destructiveAction, QMessageBox::AcceptRole /*QMessageBox::DestructiveRole*/);
	cancelButton_ = addButton(QMessageBox::Cancel);
	setDefaultButton(removeButton_);
}

void RemoveConfirmationMessageBox::setComplimentaryActionName(const QString& complimentaryAction)
{
	Q_ASSERT(removeButton_);
	Q_ASSERT(cancelButton_);
	Q_ASSERT(!infoButton_);
	Q_ASSERT(!complimentaryButton_);
	complimentaryButton_ = addButton(complimentaryAction, QMessageBox::AcceptRole);
}

void RemoveConfirmationMessageBox::setInfoActionName(const QString& infoAction)
{
	Q_ASSERT(!removeButton_);
	Q_ASSERT(!cancelButton_);
	Q_ASSERT(!infoButton_);
	infoButton_ = addButton(infoAction, QMessageBox::AcceptRole);
	setDefaultButton(infoButton_);
}

QString RemoveConfirmationMessageBox::processInformativeText(const QString& informativeText)
{
	QString text = informativeText;
	text.replace("<br>", "\n");
	QRegExp rx("<.+>");
	rx.setMinimal(true);
	text.replace(rx, "");
	return text;
}

void RemoveConfirmationMessageBox::doExec()
{
	if (!removeButton_ && !infoButton_) {
		setDestructiveActionName(tr("Delete"));
	}

	setText(processInformativeText(informativeText()));
	setInformativeText(QString());

	Q_ASSERT((removeButton_ && cancelButton_) || infoButton_);
	exec();
}

bool RemoveConfirmationMessageBox::removeAction() const
{
	return clickedButton() == removeButton_;
}

bool RemoveConfirmationMessageBox::complimentaryAction() const
{
	return clickedButton() == complimentaryButton_;
}

bool RemoveConfirmationMessageBox::infoAction() const
{
	return clickedButton() == infoButton_;
}
