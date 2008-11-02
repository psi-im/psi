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

#ifndef MCMDCOMPLETION_H
#define MCMDCOMPLETION_H

#include "tabcompletion.h"

class MCmdManagerIface;


/** This class offers TabCompletion based completion support
  * for cases where only mini commands need to be completed
  */
class MCmdTabCompletion : public TabCompletion {
public:
	/** Constructs an MCmdTabCompletion for getting completions from mini command
	  * manager \a mgr.
	  */
	MCmdTabCompletion(MCmdManagerIface *mgr) : mgr_(mgr) {};

protected:
	virtual void setup(QString str, int pos, int &start, int &end);
	virtual QStringList possibleCompletions();
	virtual QStringList allChoices(QString &guess);

	QStringList mCmdList_;
	MCmdManagerIface *mgr_;
};


#endif
