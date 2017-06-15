/*
 * statusmenu.cpp - helper class that displays available statuses using QMenu
 * Copyright (C) 2008-2010  Michail Pishchagin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "statusmenu.h"

#include "psioptions.h"
#include "psiiconset.h"
#include "psiaccount.h"
#include "xmpp_status.h"
#include "common.h"

#include <cassert>

/**
 * \class StatusMenu
 * Helper class that displays available statuses using QMenu.
 */

StatusMenu::StatusMenu(QWidget* parent, PsiCon* _psi )
	: QMenu(parent), psi(_psi)
{
	const QString css = PsiOptions::instance()->getOption("options.ui.contactlist.css").toString();
	if (!css.isEmpty()) {
		setStyleSheet(css);
	}
	connect(psi, SIGNAL(statusPresetsChanged()), this, SLOT(presetsChanged()));
	installEventFilter(this);
}

void StatusMenu::fill()
{
	PsiOptions* o = PsiOptions::instance();

	addStatus(XMPP::Status::Online);
	if (!o->getOption("options.status.show-only-online-offline").toBool()) {
		if (o->getOption("options.ui.menu.status.chat").toBool())
			addStatus(XMPP::Status::FFC);
		addSeparator();
		addStatus(XMPP::Status::Away);
		if (o->getOption("options.ui.menu.status.xa").toBool())
			addStatus(XMPP::Status::XA);
		addStatus(XMPP::Status::DND);
		if (o->getOption("options.ui.menu.status.invisible").toBool()) {
			addSeparator();
			addStatus(XMPP::Status::Invisible);
		}
	}

	QString statusInMenus = o->getOption("options.status.presets-in-status-menus").toString();
	if (statusInMenus == "submenu") {
		addSeparator();
		IconActionGroup* submenu = new IconActionGroup(this);
		submenu->setText(tr("Presets"));
		submenu->setPsiIcon("psi/action_templates");
		const QString css = o->getOption("options.ui.contactlist.css").toString();
		if (!css.isEmpty()) {
			submenu->popup()->setStyleSheet(css);
		}
		addPresets(submenu);
		submenu->popup()->installEventFilter(this);
	}
	else if (statusInMenus == "yes") {
		addPresets();
	}

	bool showChoose = o->getOption("options.status.show-choose").toBool();
	bool showReconnect = o->getOption("options.status.show-reconnect").toBool();
	if (showChoose || showReconnect) {
		addSeparator();
		if (showChoose) {
			addChoose();
		}
		if (showReconnect) {
			addReconnect();
		}
	}

	addSeparator();
	addStatus(XMPP::Status::Offline);
}

void StatusMenu::clear()
{
	statusActs.clear();
	presetActs.clear();
	QMenu::clear();
}

void StatusMenu::addPresets(IconActionGroup* parent)
{
	QObject* parentMenu = parent ? parent : static_cast<QObject*>(this);
	PsiOptions* o = PsiOptions::instance();
	QVariantList presets = o->mapKeyList("options.status.presets", true);
	bool showEditPresets = o->getOption("options.status.show-edit-presets").toBool();
	if ((showEditPresets || presets.count() > 0) && !parent)
		this->addSeparator();
	if (presets.count() > 0) {
		foreach(QVariant name, presets) {
			StatusPreset preset;
			preset.fromOptions(o,name.toString());
			preset.filterStatus();

			IconAction* action = new IconAction(name.toString().replace("&", "&&"), parentMenu, status2name(preset.status()));
			action->setCheckable(true);
			action->setProperty("preset", name);
			action->setProperty("message", preset.message());
			action->setProperty("status", QVariant(preset.status()));
			connect(action, SIGNAL(triggered()), SLOT(presetActivated()));
			presetActs.append(action);
		}
	}
	if (showEditPresets) {
		IconAction* action = new IconAction(tr("Edit presets..."), parentMenu, "psi/action_templates_edit");
		connect(action, SIGNAL(triggered()), SLOT(changePresetsActivated()));
	}
}

void StatusMenu::presetsChanged()
{
	if (PsiOptions::instance()->getOption("options.status.presets-in-status-menus").toString() != "no")
	{
		//TODO Maybe refresh only presets?...
		clear();
		fill();
	}
}

void StatusMenu::statusChanged(const Status& status)
{
	bool presetFound = false;
	foreach(IconAction* action, presetActs)
	{
		//Maybe we should compare with priority too
		int st = static_cast<int>(status.type());
		QString message = action->property("message").toString();
		if (!presetFound && action->property("status").toInt() == st && message == status.status())
		{
			action->setChecked(true);
			presetFound = true;
		}
		else
			action->setChecked(false);
	}
	bool statusFound = false;
	foreach(IconAction* action, statusActs)
	{
		if (!statusFound && !presetFound && action->property("type").toInt() == static_cast<int>(status.type()))
		{
			action->setChecked(true);
			statusFound = true;
		}
		else
			action->setChecked(false);
	}
}

void StatusMenu::presetActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	QString name = action->property("preset").toString();
	PsiOptions* o = PsiOptions::instance();
	QString base = o->mapLookup("options.status.presets", name);
	XMPP::Status status;
	status.setType(o->getOption(base + ".status").toString());
	status.setStatus(o->getOption(base + ".message").toString());
	bool withPriority = false;
	if (o->getOption(base + ".force-priority").toBool()) {
		withPriority = true;
		status.setPriority(o->getOption(base + ".priority").toInt());
	}
	if (o->getOption("options.status.last-overwrite.by-template").toBool()) {
		o->setOption("options.status.last-priority", withPriority ? QString::number(status.priority()) : "");
		o->setOption("options.status.last-message", status.status());
		o->setOption("options.status.last-status", status.typeString());
	}
	emit statusPresetSelected(status, withPriority, true);
}

void StatusMenu::changePresetsActivated()
{
	psi->doStatusPresets();
}

void StatusMenu::addStatus(XMPP::Status::Type type)
{
	IconAction* action = new IconAction(status2txt(type), this, status2name(type));
	action->setCheckable(true);
	action->setProperty("type", QVariant(type));
	connect(action, SIGNAL(triggered()), SLOT(statusActivated()));
	statusActs.append(action);
}

XMPP::Status::Type StatusMenu::actionStatus(const QAction* action) const
{
	Q_ASSERT(action);
	return static_cast<XMPP::Status::Type>(action->property("type").toInt());
}

void StatusMenu::statusActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	XMPP::Status::Type status = actionStatus(action);
	emit statusSelected(status, false);
}

bool StatusMenu::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent* e = static_cast<QMouseEvent*>(event); //We sure event is QMouseEvent
		QMenu* menu = dynamic_cast<QMenu*>(obj); //Event filter can be installed on anything, so do dynamic_cast
		assert(menu != 0); //Dynamic cast on wrong type will return 0
		QAction* action = menu->actionAt(e->pos());
		if (action && e->button() == Qt::RightButton)
		{
			if (!action->property("type").isNull())
			{
				XMPP::Status::Type status = actionStatus(action);
				emit statusSelected(status, true);
				return true;
			}
			else if (!action->property("preset").isNull())
			{
				QString presetName = action->property("preset").toString();
				emit statusPresetDialogForced(presetName);
				return true;
			}
		}
	}
	return false;
}
