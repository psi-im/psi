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

// tab completion helper for mini command system

#include "mcmdcompletion.h"
#include "minicmd.h"

void MCmdTabCompletion::setup(QString str, int pos, int &start, int &end) {
	if (mgr_->isActive()) {
		mCmdList_ = mgr_->completeCommand(str, pos, start, end);
	} else {
		TabCompletion::setup(str, pos, start, end);
	}
}

QStringList MCmdTabCompletion::possibleCompletions() {
	if (mgr_->isActive()) {
		return mCmdList_;
	}
	return QStringList();
}

QStringList MCmdTabCompletion::allChoices(QString &guess) {
	if (mgr_->isActive()) {
		guess = QString();
		return mCmdList_;
	}
	return QStringList();
}
