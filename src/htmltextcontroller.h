/*
 * htmltextcontroller.h
 * Copyright (C) 2010  Evgeny Khryukin
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
#ifndef HTMLTEXTCONTROLLER_H
#define HTMLTEXTCONTROLLER_H

#include  <QTextEdit>

class HTMLTextController : public QObject
{
	Q_OBJECT
public:
	enum TextEditState {
		StateNone = 0,
		StateBold = 1,
		StateItalic = 2,
		StateUnderline = 4,
		StateTextStyleChanged = 8,
		StateTextColorChanged = 16,
		StateBackgroundColorChanged = 32,
		StateStrikeOut = 64
	};

	HTMLTextController(QTextEdit *parent);
	void doMenu();
	void setFont(const QFont &);
	void setCssString(const QString& css) { cssString_ = css; };

private:
	void addState(TextEditState state);
	void removeState(TextEditState state);
	QList<TextEditState> state();
	QString cssString_;

private:
	QTextEdit *te_;
	QFont font_, currentFont_;
	QBrush background_ ,currentBackground_;
	QBrush foreground_, currentForeground_;
};

#endif // HTMLTEXTCONTROLLER_H
