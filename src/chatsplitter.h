/*
 * chatsplitter.h - QSplitter replacement that masquerades it
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CHATSPLITTER_H
#define CHATSPLITTER_H

#include <QWidget>
#include <QList>

class QSplitter;

class ChatSplitter : public QWidget
{
	Q_OBJECT
public:
	ChatSplitter(QWidget* parent);

	void setOrientation(Qt::Orientation orientation);
	void addWidget(QWidget* widget);
	void setSizes(const QList<int>& list);

protected:
	/**
	 * Returns true if all child widgets are managed by QLayout.
	 */
	bool splitterEnabled() const { return splitterEnabled_; }
	void setSplitterEnabled(bool enable);

public slots:
	void optionsChanged();

private slots:
	void childDestroyed(QObject* obj);

private:
	void updateChildLayout(QWidget* child);
	void updateLayout();

	bool splitterEnabled_;
	QList<QWidget*> children_;
	QSplitter* splitter_;
	QLayout* layout_;
};

#endif
