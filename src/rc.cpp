/*
 * rc.cpp - Implementation of JEP-146 (Remote Controlling Clients)
 * Copyright (C) 2005  Remko Troncon
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

#include "iconaction.h"
#include "psiaccount.h"
#include "psiactionlist.h"
#include "psicon.h"
#include "psioptions.h"
#include "rc.h"
#include "xmpp_xdata.h"
#include "ahcservermanager.h"
#include "ahcommand.h"
#include "groupchatdlg.h"

using namespace XMPP;

bool RCCommandServer::isAllowed(const Jid& j) const
{
	return manager()->account()->jid().compare(j,false);
}

AHCommand RCSetStatusServer::execute(const AHCommand& c, const Jid&)
{
	// Check if the session ID is correct
	//if (c.sessionId() != "")
	//	return AHCommand::errorReply(c,AHCError::AHCError(AHCError::BadSessionID));

	if (!c.hasData()) {
		// Initial set status form
		XData form;
		form.setTitle(QObject::tr("Set Status"));
		form.setInstructions(QObject::tr("Choose the status and status message"));
		form.setType(XData::Data_Form);
		XData::FieldList fields;

		XData::Field type_field;
		type_field.setType(XData::Field::Field_Hidden);
		type_field.setVar("FORM_TYPE");
		type_field.setValue(QStringList("http://jabber.org/protocol/rc"));
		type_field.setRequired(false);
		fields += type_field;

		XData::Field status_field;
		status_field.setType(XData::Field::Field_ListSingle);
		status_field.setVar("status");
		status_field.setLabel(QObject::tr("Status"));
		status_field.setRequired(true);
		status_field.setValue(QStringList(manager()->account()->status().typeString()));
		XData::Field::OptionList status_options;
		if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool()) {
			XData::Field::Option chat_option;
			chat_option.label = QObject::tr("Chat");
			chat_option.value = "chat";
			status_options += chat_option;
		}
		XData::Field::Option online_option;
		online_option.label = QObject::tr("Online");
		online_option.value = "online";
		status_options += online_option;
		XData::Field::Option away_option;
		away_option.label = QObject::tr("Away");
		away_option.value = "away";
		status_options += away_option;
		if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool()) {
			XData::Field::Option xa_option;
			xa_option.label = QObject::tr("Extended Away");
			xa_option.value = "xa";
			status_options += xa_option;
		}
		XData::Field::Option dnd_option;
		dnd_option.label = QObject::tr("Do Not Disturb");
		dnd_option.value = "dnd";
		status_options += dnd_option;
		if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
			XData::Field::Option invisible_option;
			invisible_option.label = QObject::tr("Invisible");
			invisible_option.value = "invisible";
			status_options += invisible_option;
		}
		XData::Field::Option offline_option;
		offline_option.label = QObject::tr("Offline");
		offline_option.value = "offline";
		status_options += offline_option;
		status_field.setOptions(status_options);
		fields += status_field;

		XData::Field priority_field;
		priority_field.setType(XData::Field::Field_TextSingle);
		priority_field.setLabel(QObject::tr("Priority"));
		priority_field.setVar("status-priority");
		priority_field.setRequired(false);
		priority_field.setValue(QStringList(QString::number(manager()->account()->status().priority())));
		fields += priority_field;

		XData::Field statusmsg_field;
		statusmsg_field.setType(XData::Field::Field_TextMulti);
		statusmsg_field.setLabel(QObject::tr("Message"));
		statusmsg_field.setVar("status-message");
		statusmsg_field.setRequired(false);
		statusmsg_field.setValue(QStringList(manager()->account()->status().status()));
		fields += statusmsg_field;

		form.setFields(fields);

		return AHCommand::formReply(c, form);
	}
	else {
		// Set the status
		Status s;
		bool foundStatus = false;
		XData::FieldList fl = c.data().fields();
		for (int i=0; i < fl.count(); i++) {
			if (fl[i].var() == "status" && !(fl[i].value().isEmpty())) {
				foundStatus = true;
				s.setType(fl[i].value().first());
			}
			else if (fl[i].var() == "status-message" && !fl[i].value().isEmpty()) {
				s.setStatus(fl[i].value().join("\n"));
			}
			else if (fl[i].var() == "status-priority" && !fl[i].value().isEmpty()) {
				s.setPriority(fl[i].value().first().toInt());
			}
		}
		if (foundStatus) {
			manager()->account()->setStatus(s, true, true);
		}
		return AHCommand::completedReply(c);
	}
}


AHCommand RCForwardServer::execute(const AHCommand& c, const Jid& j)
{
	int messageCount = manager()->account()->forwardPendingEvents(j);
	XData form;
	form.setTitle(QObject::tr("Forward Messages"));
	form.setInstructions(QObject::tr("Forwarded %1 messages").arg(messageCount));
	form.setType(XData::Data_Form);
	return AHCommand::completedReply(c,form);
}

AHCommand RCSetOptionsServer::execute(const AHCommand& c, const Jid&)
{
	if (!c.hasData()) {
		// Initial set options form
		XData form;
		form.setTitle(QObject::tr("Set Options"));
		form.setInstructions(QObject::tr("Set the desired options"));
		form.setType(XData::Data_Form);
		XData::FieldList fields;

		XData::Field type_field;
		type_field.setType(XData::Field::Field_Hidden);
		type_field.setVar("FORM_TYPE");
		type_field.setValue(QStringList("http://jabber.org/protocol/rc"));
		type_field.setRequired(false);
		fields += type_field;

		XData::Field sounds_field;
		sounds_field.setType(XData::Field::Field_Boolean);
		sounds_field.setLabel(QObject::tr("Play sounds"));
		sounds_field.setVar("sounds");
		sounds_field.setValue(QStringList((PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool() ? "1" : "0")));
		sounds_field.setRequired(false);
		fields += sounds_field;

		XData::Field auto_offline_field;
		auto_offline_field.setType(XData::Field::Field_Boolean);
		auto_offline_field.setLabel(QObject::tr("Automatically go offline when idle"));
		auto_offline_field.setVar("auto-offline");
		auto_offline_field.setValue(QStringList((PsiOptions::instance()->getOption("options.status.auto-away.use-offline").toBool() ? "1" : "0")));
		auto_offline_field.setRequired(false);
		fields += auto_offline_field;

		XData::Field auto_auth_field;
		auto_auth_field.setType(XData::Field::Field_Boolean);
		auto_auth_field.setLabel(QObject::tr("Auto-authorize contacts"));
		auto_auth_field.setVar("auto-auth");
		auto_auth_field.setValue(QStringList((PsiOptions::instance()->getOption("options.subscriptions.automatically-allow-authorization").toBool() ? "1" : "0")));
		auto_auth_field.setRequired(false);
		fields += auto_auth_field;

		XData::Field auto_open_field;
		auto_open_field.setType(XData::Field::Field_Boolean);
		auto_open_field.setLabel(QObject::tr("Auto-open new messages"));
		auto_open_field.setVar("auto-open");
		auto_open_field.setValue(QStringList((PsiOptions::instance()->getOption("options.ui.message.auto-popup").toBool() ? "1" : "0")));
		auto_open_field.setRequired(false);
		fields += auto_open_field;

		form.setFields(fields);

		return AHCommand::formReply(c, form);
	}
	else {
		// Set the options
		XData::FieldList fl = c.data().fields();
		for (int i=0; i < fl.count(); i++) {
			if (fl[i].var() == "sounds") {
				QString v =  fl[i].value().first();
				IconAction* soundact = psiCon_->actionList()->suitableActions(PsiActionList::ActionsType( PsiActionList::Actions_MainWin | PsiActionList::Actions_Common)).action("menu_play_sounds");
				if (v == "1")
					soundact->setChecked(true);
				else if (v == "0")
					soundact->setChecked(false);
			}
			else if (fl[i].var() == "auto-offline") {
				QString v =  fl[i].value().first();
				if (v == "1")
					PsiOptions::instance()->setOption("options.status.auto-away.use-offline", (bool) true);
				else if (v == "0")
					PsiOptions::instance()->setOption("options.status.auto-away.use-offline", (bool) false);
			}
			else if (fl[i].var() == "auto-auth") {
				QString v =  fl[i].value().first();
				if (v == "1")
					PsiOptions::instance()->setOption("options.subscriptions.automatically-allow-authorization", (bool) true);
				else if (v == "0")
					PsiOptions::instance()->setOption("options.subscriptions.automatically-allow-authorization", (bool) false);
			}
			else if (fl[i].var() == "auto-open") {
				QString v =  fl[i].value().first();
				if (v == "1")
					PsiOptions::instance()->setOption("options.ui.message.auto-popup", (bool) true);
				else if (v == "0")
					PsiOptions::instance()->setOption("options.ui.message.auto-popup", (bool) false);
			}
		}
		return AHCommand::completedReply(c);
	}
}

AHCommand RCLeaveMucServer::execute(const AHCommand& c, const Jid& /*j*/)
{
	foreach (QString gc, manager()->account()->groupchats()) {
		Jid mj(gc);
		GCMainDlg *gcDlg = manager()->account()->findDialog<GCMainDlg*>(mj.bare());
		if (gcDlg) gcDlg->close();
	}
	XData form;
	form.setTitle(QObject::tr("Leave All Conferences"));
	form.setType(XData::Data_Form);
	return AHCommand::completedReply(c);
}
