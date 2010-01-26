/*
 * historydlg.h - a dialog to show event history
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef HISTORYDLG_H
#define HISTORYDLG_H

#include <q3listview.h>

class PsiEvent;
class PsiAccount;
class EDBItem;
class EDBResult;
class Q3SimpleRichText;
namespace XMPP {
	class Jid;
}

class HistoryViewItem : public Q3ListViewItem
{
public:
	HistoryViewItem(PsiEvent *, const QString &, int id, Q3ListView *);
	~HistoryViewItem();

	Q3SimpleRichText *rt;
	QString text;
	int id;
	PsiEvent *e;
	QString eventId;

	// reimplemented
	int rtti() const;
	void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment);
	void setup();
	int compare(Q3ListViewItem *, int column, bool ascending) const;
};

class HistoryView : public Q3ListView
{
	Q_OBJECT
public:
	HistoryView(QWidget *parent=0, const char *name=0);

	void doResize();
	void addEvent(PsiEvent *, const QString &);

	// reimplemented
	void keyPressEvent(QKeyEvent *e);
	void resizeEvent(QResizeEvent *e);

signals:
	void aOpenEvent(PsiEvent *);

private slots:
	void doOpenEvent();
	void doCopyEvent();

	void qlv_doubleclick(Q3ListViewItem *);
	void qlv_contextPopup(Q3ListViewItem *, const QPoint &, int);

private:
	int at_id;
};

class HistoryDlg : public QWidget
{
	Q_OBJECT
public:
	HistoryDlg(const XMPP::Jid &, PsiAccount *);
	~HistoryDlg();

	// reimplemented
	void keyPressEvent(QKeyEvent *e);
	void closeEvent(QCloseEvent *e);

signals:
	void openEvent(PsiEvent *);

public slots:
	// reimplemented
	void show();

private slots:
	void doLatest();
	void doPrev();
	void doNext();
	void doSave();
	void doErase();
	void setButtons();
	void actionOpenEvent(PsiEvent *);
	void doFind();

	void edb_finished();
	void le_textChanged(const QString &);

private:
	class Private;
	Private *d;

	void loadPage(int);
	void displayResult(const EDBResult *, int, int max=-1);
	void exportHistory(const QString &fname);
};

#endif
