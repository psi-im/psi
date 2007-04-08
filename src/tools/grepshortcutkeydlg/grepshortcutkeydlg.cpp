/*
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

GrepShortcutKeyDlg::GrepShortcutKeyDlg()
	: QDialog()
	, gotKey(false)
{
	ui_.setupUi(this);
	displayPressedKeys(QKeySequence());
}

/**
 * Grabs the keyboard and proceeds with the default show() call.
 */
void GrepShortcutKeyDlg::show()
{
	grabKeyboard();
	QDialog::show();
}

/**
 * Releases the grabbed keyboard and proceeds with the default close() call.
 */
void GrepShortcutKeyDlg::close()
{
	QDialog::close();
	releaseKeyboard();
}

void GrepShortcutKeyDlg::displayPressedKeys(QKeySequence keys)
{
	QString str = keys.toString(QKeySequence::NativeText);
	if (str.isEmpty())
		str = tr("Set Keys");
	ui_.shortcutPreview->setText(str);
}

QKeySequence GrepShortcutKeyDlg::getKeySequence(QKeyEvent* event) const
{
	return QKeySequence((isValid(event->key()) ? event->key() : 0)
	                    + (event->modifiers() & ~Qt::KeypadModifier));
}

void GrepShortcutKeyDlg::keyPressEvent(QKeyEvent* event)
{
	displayPressedKeys(getKeySequence(event));

	if (!isValid(event->key()) || gotKey)
		return;

	gotKey = true;
	emit newShortcutKey(getKeySequence(event));
	close();
}

void GrepShortcutKeyDlg::keyReleaseEvent(QKeyEvent* event)
{
	displayPressedKeys(getKeySequence(event));
}

/**
 * Returns true if \param key could be used in a shortcut.
 */
bool GrepShortcutKeyDlg::isValid(int key) const
{
	switch (key) {
	case 0:
	case Qt::Key_unknown:
		return false;
	}

	return !isModifier(key);
}

/**
 * Returns true if \param key is modifier.
 */
bool GrepShortcutKeyDlg::isModifier(int key) const
{
	switch (key) {
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
