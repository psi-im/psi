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

// Interfaces for mini command system



#ifndef MINICMD_H
#define MINICMD_H

#include <QHash>

class QString;
class QStringList;
class MCmdProviderIface;

/** unparsed state: this is a state where the whole user input is passed
  * unparsed as the first part to the handler functions
  */
#define MCMDSTATE_UNPARSED	1

/** This interface models the methods common to all mini command states.
  */
class MCmdStateIface {
public:
	/** \return the name of the state
	 */
	virtual QString getName()=0;

	/** \return the prompt of the state
	  */
	virtual QString getPrompt()=0;

	/** \return flags for this state
	  */
	virtual int getFlags()=0;

	/** Called when the state is no longer needed to free associated memory.
	 */
	virtual void dispose()=0;

	/** Called when no more-specific state transition handler handled the command.
	 */
	virtual bool unhandled(QStringList command)=0;

	/** \return additional data for this state
	 */
	virtual const QHash<QString, QVariant> &getInfo()=0;

	virtual ~MCmdStateIface() {};
};

/** Interface for user interface site of an mini command.
 *  For every MCmdManager instance has one instance of this interface to
 *  handle interaction with the user interface.
 *
 *  The user interface has two states: closed or open. If the user interface
 *  is open, it has a prompt shown to the user and some current input text.
 */
class MCmdUiSiteIface { // per interaction site
public:
	/** Open user interface.
	 *  The site will open the user interface with displaying \a prompt as prompt and
	 *  inizialising the input box with \a def.
	 */
	virtual void mCmdReady(const QString prompt, const QString def)=0;

	/** Close user interface.
	 *  The site closes the user interface.
	 */
	virtual void mCmdClose()=0;

	virtual ~MCmdUiSiteIface() {};
};

/** Interface for the mini command manager.
 *  This manager implements the core of the mini command state machine.
 *  It uses an MCmdUiSite for it's interactions with the user interface and dispatches
 *  commands and state transitions to MCmdProvider:s registered with it.
 *
 *  Each manager with has one user interface site.
 */
class MCmdManagerIface { // per interaction site
public:
	/** Processes a raw command from the user.
	 *  This method parses the command \a command and initiates the
	 *  state change for it.
	 *  \return FIXME needed?
	 */
	virtual bool processCommand(QString command)=0;

	/** Find possible tab complations on the command \a command with cursor on position \a pos.
	 *
	 *  \return a list of possible replacements of the text between \a start and \a end.
	 */
	virtual QStringList completeCommand(QString &command, int pos, int &start, int &end)=0;

	/** Starts the state machine in the state \a state with a current
	 *  text of \a preset.
	 */
	virtual bool open(MCmdStateIface *state, QStringList preset)=0;

	/** Returns true if the manager is in a non null state; otherwise returns false. */
	virtual bool isActive()=0;

	// Provider registratation
	/** Registers a provider \a prov with this manager.
	 */
	virtual void registerProvider(MCmdProviderIface *prov)=0;

	virtual ~MCmdManagerIface(){};
};

/** Interface for command providers for a mini command manager.
 *  This interface is used to supply the state transitions and command execution
 *  for a mini command area.
 *  It defines possible states and their transitions.
 */
class MCmdProviderIface { // multiple independent users possible
public:
	/** Called if a command is executed.
	 *  if oldstate is the well known state MCMD_STARTSTATE, the command that
	 *  was parsed to the argument list \a command was issued on the start prompt.
	 *  If the command is complete this method should set newstate to the well
	 *  known state MCMD_DONESTATE, otherwise, it sets \a newstate to the state
	 *  for the next prompt and \a preset to text to initially display
	 *  in the input box. The provider if \a newstate will get a mCmdInitState
	 *  call to setup this state.
	 */
	virtual bool mCmdTryStateTransit(MCmdStateIface *oldstate, QStringList command, MCmdStateIface *&newstate, QStringList &preset)=0;

	/** Called if the user requests tab completion while in state \a state.
	 *  \a query is the word to complete (from the start of the word to the
	 *  cursor). If the provider needs more context to find the list of
	 *  possible completions \a partcommand contains the parsed current
	 *  commandline and \a item indicates which part contains the cursor.
	 *
	 * \return a QStringList of all possible completions. If a completion ends with a null char,
	 *           an unquoted/unescaped space will be added at the end of the completion. Ordering is irrelevant.
	 */
	virtual QStringList mCmdTryCompleteCommand(MCmdStateIface *state, QString query, QStringList partcommand, int item)=0;

	/** FIXME
	 */
	virtual void mCmdSiteDestroyed()=0; // once per registerProvider

	virtual ~MCmdProviderIface() {};
};

#endif
