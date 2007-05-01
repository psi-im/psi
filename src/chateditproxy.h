/*
 * chateditproxy.h - abstraction to change ChatEdit type in runtime
 * Copyright (C) 2007  Michail Pishchagin
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

#ifndef CHATEDITPROXY_H
#define CHATEDITPROXY_H

#include <QWidget>

class QTextEdit;
class QLayout;
class ChatEdit;

class ChatEditProxy : public QWidget
{
	Q_OBJECT
public:
	ChatEditProxy(QWidget* parent);

	/**
	 * Returns encapsulated QTextEdit widget.
	 */
	ChatEdit* chatEdit() const { return textEdit_; }

signals:
	/**
	 * Emitted when internal QTextEdit gets replaced with
	 * another one.
	 */
	void textEditCreated(QTextEdit* textEdit);

protected:
	/**
	 * Returns true if line edit mode is enabled.
	 */
	bool lineEditEnabled() const { return lineEditEnabled_; }
	void setLineEditEnabled(bool enable);

public slots:
	void optionsChanged();

private:
	virtual ChatEdit* createTextEdit();
	void moveData(QTextEdit* newTextEdit, QTextEdit* oldTextEdit) const;
	void updateLayout();

	bool lineEditEnabled_;
	ChatEdit* textEdit_;
	QLayout* layout_;
};

#endif
