/*
 * typeaheadfind.h - Typeahead find toolbar
 * Copyright (C) 2006  Maciej Niedzielski
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

#ifndef TYPEAHEADFIND_H
#define TYPEAHEADFIND_H

#include <QToolBar>

#ifdef WEBKIT
#include <webview.h>
#endif

class QTextEdit;
class QString;

class TypeAheadFindBar : public QToolBar
{
	Q_OBJECT
public:
	enum Type {
		TextEditType,
		WebViewType
	};

	TypeAheadFindBar(QTextEdit *textedit, const QString &title, QWidget *parent = 0);
#ifdef WEBKIT
	TypeAheadFindBar(WebView *webView, const QString &title, QWidget *parent = 0);
#endif
	~TypeAheadFindBar();
	void init();

public slots:
	void open();
	void close();
	void toggleVisibility();
	void optionsUpdate();

signals:
	void visibilityChanged(bool visible);

private slots:
	void textChanged(const QString &);
	void findNext();
	void findPrevious();
	void caseToggled(int);

private:
	class Private;
	Private *d;
};

#endif
