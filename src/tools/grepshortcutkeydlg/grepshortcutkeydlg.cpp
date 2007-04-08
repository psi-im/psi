/*
 *
 * grepshortcutkeydlg.cpp - a dialog which greps a KeySequence and
 * emits a signal with this KeySequence as Parameter
 * Copyright (C) 2006 Cestonaro Thilo
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
#include "grepshortcutkeydlg.h"

/**
 * \brief	keyPressEvent, is called when a Key was pressed while the dialog has the focus
 *			it ignores hopefully any Modifierkeys e.g. Ctrl and any unusally keys
 *			if a correct KeySequence is reconned, it emits the Signal newShortcutKey with
 *			with the KeySequence as param
 * \param	The KeyEvent holds informations about the pressed keys
 */
void grepShortcutKeyDlg::keyPressEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_unknown || event->key() == 0 || isModifier(event->key()) || gotKey == true)
		return;

	gotKey = true;
	emit newShortcutKey(QKeySequence(event->key() + ( event->modifiers() & ~Qt::KeypadModifier)));
	close();
}

/**
 * \brief	isModifier, checks if the given key is a modifier e.g. Ctrl and returns true respectivley false
 * \param	iKey, is the integer of the key which should be checked
 * \return	true, if iKey is a Modifier
 *			false, if iKey is not a Modifier
 */
bool grepShortcutKeyDlg::isModifier(int key) {
	switch(key) {
		case Qt::Key_Shift:
		case Qt::Key_Control:
		case Qt::Key_Meta:
		case Qt::Key_Alt:
		case Qt::Key_AltGr:
		case Qt::Key_Super_L:
		case Qt::Key_Super_R:
		case Qt::Key_Menu:
			return true;
	}
	return false;
}
