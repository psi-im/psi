/*
 * grepshortcutkeydialog.cpp - a dialog which greps a KeySequence and
 * emits a signal with this KeySequence as Parameter
 * Copyright (C) 2006  Cestonaro Thilo
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "grepshortcutkeydialog.h"

GrepShortcutKeyDialog::GrepShortcutKeyDialog() : QDialog(), gotKey(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui_.setupUi(this);
    setWindowTitle(tr("Press shortcut"));
    displayPressedKeys(QKeySequence());
}

/**
 * Grabs the keyboard and proceeds with the default show() call.
 */
void GrepShortcutKeyDialog::show()
{
    QDialog::show();
    grabKeyboard();
    setFocus();
}

/**
 * Releases the grabbed keyboard and proceeds with the default close() call.
 */
void GrepShortcutKeyDialog::closeEvent(QCloseEvent *event)
{
    releaseKeyboard();
    event->accept();
}

void GrepShortcutKeyDialog::displayPressedKeys(const QKeySequence &keys)
{
    QString str = keys.toString(QKeySequence::NativeText);
    if (str.isEmpty())
        str = tr("Set Keys");
    ui_.shortcutPreview->setText(str);
}

QKeySequence GrepShortcutKeyDialog::getKeySequence(QKeyEvent *event) const
{
    return QKeySequence((isValid(event->key()) ? event->key() : 0) | (event->modifiers() & ~Qt::KeypadModifier));
}

void GrepShortcutKeyDialog::keyPressEvent(QKeyEvent *event)
{
    displayPressedKeys(getKeySequence(event));

    if (!isValid(event->key()) || gotKey)
        return;

    gotKey = true;
    emit newShortcutKey(getKeySequence(event));
    close();
}

void GrepShortcutKeyDialog::keyReleaseEvent(QKeyEvent *event) { displayPressedKeys(getKeySequence(event)); }

/**
 * Returns true if \param key could be used in a shortcut.
 */
bool GrepShortcutKeyDialog::isValid(int key) const
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
bool GrepShortcutKeyDialog::isModifier(int key) const
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
