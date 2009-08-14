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

// simple UI integration for mini command system.

#include <QLabel>
#include <QTextEdit>
#include <QPalette>
#include <QString>
#include <QToolTip>
#include "mcmdsimplesite.h"

void MCmdSimpleSite::mCmdReady(const QString prompt, const QString def)
{
	if (!open) mini_msg_swap = inputWidget->toPlainText();
	open = true;
	promptWidget->setText(prompt);
    inputWidget->setText(def);
    inputWidget->setPalette(cmdPalette);
    promptWidget->show();
}

void MCmdSimpleSite::mCmdClose() {
	open = false;
	inputWidget->setText(mini_msg_swap);
	inputWidget->setPalette(palette);
	promptWidget->hide();
}

void MCmdSimpleSite::setInput(QTextEdit *i) {
	inputWidget = i;
	palette = inputWidget->palette();
	cmdPalette = palette;
	cmdPalette.setBrush(QPalette::Base, QToolTip::palette().brush(QPalette::ToolTipBase));
	cmdPalette.setBrush(QPalette::Text, QToolTip::palette().brush(QPalette::ToolTipText));
}
