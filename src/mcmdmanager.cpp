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

// manager mini command system

#include <QDebug>

#include "mcmdmanager.h"


MCmdSimpleState::MCmdSimpleState(QString name, QString prompt) {
	name_ = name;
	prompt_ = prompt;
}

MCmdSimpleState::MCmdSimpleState(QString name, QString prompt, int flags) {
	name_ = name;
	prompt_ = prompt;
	flags_ = flags;
}


MCmdSimpleState::~MCmdSimpleState() {
}

MCmdManager::MCmdManager(MCmdUiSiteIface* site_) : state_(0), uiSite_(site_) {
};

MCmdManager::~MCmdManager() {
	foreach(MCmdProviderIface *prov, providers_) {
		prov->mCmdSiteDestroyed();
	}
}


QStringList MCmdManager::parseCommand(const QString command, int pos, int &part, QString &partial, int &start, int &end, char &quotedAtPos)
{
	QStringList list;
	QString item;
	bool escape=false;
	int quote=0; // "=1, '=2
	bool space=true;

	int partStart = 0;
	quotedAtPos = 0;
	for (int i=0; i < command.length(); i++) {
		if (i == pos) {
			part = list.size();
			partial = item;
			end = i;
			start = partStart;
			if (quote) quotedAtPos =  (quote == 1) ? '"' : '\'';
		}


		QChar ch = command[i];
		if (escape) {
			escape = false;
			item += ch;
		} else if (ch == '\\') {
			escape = true;
		} else if (quote != 0) {
			if ((quote == 1 && ch == '"') || (quote == 2 && ch == '\'')) {
				quote = 0;
			} else {
				item += ch;
			}
		} else if (ch == ' ') {
			partStart = i+1;
			if (space) {
				continue;
			}
			list << item;
			item = "";
			space = true;
			continue;
		} else if (ch == '\'') {
			quote = 2;
		} else if (ch == '"') {
			quote = 1;
		} else {
			item += ch;
		}
		space = false;
	}
	if (command.length() == pos) {
		part = list.size();
		partial = item;
		end = command.length();
		start = partStart;
		if (quote) quotedAtPos =  (quote == 1) ? '"' : '\'';
	}

	if (!space) list << item;
	return list;
}

QString MCmdManager::serializeCommand(const QStringList &list)
{
	QString retval;
	bool needspace = false;
	QRegExp specials("([\"\'\\\\ ])");
	foreach(QString item, list) {
		item.replace(specials, "\\\\1");
		if (item == "") item = "\"\"";
		if (needspace) retval += " ";
		retval += item;
		needspace = true;
	}
	return retval;
}


bool MCmdManager::processCommand(QString command) {
	MCmdStateIface *tmpstate=0;
	QStringList preset;
	QStringList items;
	if (state_->getFlags() & MCMDSTATE_UNPARSED) {
		items << command;
	} else {
		int tmp_1;
		QString tmp_2;
		char tmp_3;
		items = parseCommand(command, -1, tmp_1, tmp_2, tmp_1, tmp_1, tmp_3);
	}
	foreach(MCmdProviderIface *prov, providers_) {
		if (prov->mCmdTryStateTransit(state_, items, tmpstate, preset)) {
			state_ = tmpstate;
			if (state_ != 0) {
				QString prompt = state_->getPrompt();
				QString def;
				if (state_->getFlags() & MCMDSTATE_UNPARSED) {
					if (preset.size() == 1) {
						def = preset.at(0);
					}
				} else {
					def = serializeCommand(preset);
				}
				uiSite_->mCmdReady(prompt, def);
			} else {
				uiSite_->mCmdClose();
			}
			return true;
		}
	}

	tmpstate = state_;
	bool ret = state_->unhandled(items);
	state_ = 0;
	if (state_ == 0) {
		tmpstate->dispose();
		uiSite_->mCmdClose();
	}
	return ret;
}


bool MCmdManager::open(MCmdStateIface *state, QStringList preset) {
	if (0 != state_) state_->dispose();

	state_ = state;
	QString prompt = state->getPrompt();
	QString def;
	if (state_->getFlags() & MCMDSTATE_UNPARSED) {
		if (preset.size() == 1) def = preset.at(0);
	} else {
		def = serializeCommand(preset);
	}
	uiSite_->mCmdReady(prompt, def);
	return true;
}


QStringList MCmdManager::completeCommand(QString &command, int pos, int &start, int &end) {
	int part;
	QString query;
	char quotedAtPos;
	QStringList all;
	if (state_->getFlags() & MCMDSTATE_UNPARSED) {
		all << command;
		query = command.left(pos);
		part = -1;
	} else {
		all = parseCommand(command, pos, part, query, start, end, quotedAtPos);
	}

	QStringList res;
	foreach(MCmdProviderIface *prov, providers_) {
		res += prov->mCmdTryCompleteCommand(state_, query, all, part);
	}
	res.sort();

	QStringList quoted;
	if ((state_->getFlags() & MCMDSTATE_UNPARSED) == 0) {
		foreach(QString str, res) {
			QString trail;
			if (str.size() > 1 && str.at(str.size()-1) == QChar(0)) {
				str.chop(1);
				trail = " ";
			}
			str = str.replace("\\", "\\\\");
			if (quotedAtPos == 0) {
				quoted << str.replace(" ", "\\ ").replace("\"", "\\\"").replace("'", "\\'") + trail;
			} else {
				quoted << quotedAtPos + str.replace(quotedAtPos, QString("\\") + quotedAtPos) + trail;
			}
		}
	} else {
		quoted = res;
	}
	return quoted;
}

bool MCmdManager::isActive() {
	return state_ != 0;
}



void MCmdManager::registerProvider(MCmdProviderIface *prov)
{
	if (! providers_.contains(prov)) {
		providers_ += prov;
	}
}
