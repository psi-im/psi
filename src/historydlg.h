/*
 * historydlg.h
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Evgeny Khryukin
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

#ifndef HISTORYDLG_H
#define HISTORYDLG_H

#include "eventdb.h"
#include "ui_history.h"
#include "advwidget.h"
#include "historycontactlistmodel.h"

class PsiAccount;
class PsiContact;
class UserListItem;

namespace XMPP {
	class Jid;
}

class DisplayProxy;

class SearchProxy : public QObject
{
	Q_OBJECT

public:
	SearchProxy(PsiCon *p, DisplayProxy *d);
	void find(const QString &str, const QString &acc_id, const XMPP::Jid &jid, int dir);
	int totalFound() const { return total_found; }
	int cursorPosition() const { return general_pos; }

private slots:
	void handleResult();

signals:
	void found(int);
	void needRequest();

private:
	struct Position {
		QDateTime date;
		int       num;
	} position;
	void reset();
	EDBHandle* getEDBHandle();
	void movePosition(int dir);
	int  invertSearchPosition(const Position &pos, int dir);
	void handleFoundData(const EDBResult &r);
	void handlePadding(const EDBResult &r);

private:
	int general_pos;
	int total_found;
	bool active;
	int direction;
	QString s_string;
	QHash<uint, int> map;
	QVector<Position> list;
	PsiCon *psi;
	DisplayProxy *dp;
	QString acc_;
	XMPP::Jid jid_;
	enum RequestType { ReqFind, ReqPadding };
	RequestType reqType;
};

class DisplayProxy : public QObject
{
	Q_OBJECT

public:
	DisplayProxy(PsiCon *p, PsiTextView *v);

	void setEmoticonsFlag(bool f) { emoticons = f; }
	void setFormattingFlag(bool f) { formatting = f; }
	void setSentColor(const QString &c) { sentColor = c; }
	void setReceivedColor(const QString &c) { receivedColor = c; }

	bool canBackward() const { return can_backward; }
	bool canForward() const { return can_forward; }

	void displayEarliest(const QString &acc_id, const XMPP::Jid &jid);
	void displayLatest(const QString &acc_id, const XMPP::Jid &jid);
	void displayFromDate(const QString &acc_id, const XMPP::Jid &jid, const QDateTime date);
	void displayNext();
	void displayPrevious();
	bool moveSearchCursor(int dir, int n);
	void displayWithSearchCursor(const QString &acc_id, const XMPP::Jid &jid, QDateTime start, int dir, const QString &s_str, int num);
	bool isMessage(const QTextCursor &cursor) const;

private slots:
	void handleResult();

signals:
	void updated();
	void searchCursorMoved();

private:
	EDBHandle* getEDBHandle();
	void resetSearch();
	void updateQueryParams(int dir, int increase, QDateTime date = QDateTime());
	void displayResult(const EDBResult &r, int dir);
	QString getNick(PsiAccount *pa, const XMPP::Jid &jid) const;

private:
	QString acc_;
	XMPP::Jid jid_;
	struct {
		int       direction;
		int       offset;
		QDateTime date;
	} queryParams;
	struct {
		int     searchPos;
		int     cursorPos;
		int     searchDir;
		QString searchString;
	} searchParams;
	enum RequestType { ReqNone, ReqDate, ReqEarliest, ReqLatest, ReqNext, ReqPrevious };
	RequestType reqType;
	PsiCon *psi;
	PsiTextView *viewWid;
	bool formatting;
	bool emoticons;
	bool can_backward;
	bool can_forward;
	QString sentColor;
	QString receivedColor;
};

class HistoryDlg : public AdvancedWidget<QDialog>
{
	Q_OBJECT

public:
	HistoryDlg(const XMPP::Jid&, PsiAccount*);
	virtual ~HistoryDlg();

private slots:
	void openSelectedContact();
	void getLatest();
	void getEarliest();
	void getPrevious();
	void getNext();
	void getDate();
	void refresh();
	void findMessages();
	void edbFinished();
	void highlightBlocks();
	void changeAccount(const QString accountName);
	void removeHistory();
	void exportHistory();
	void openChat();
	void doMenu();
	void removedContact(PsiContact*);
	void optionUpdated(const QString& option);
#ifndef HAVE_X11
	void autoCopy();
#endif
	void viewUpdated();
	void showFoundResult(int rows);
	void updateSearchHint();
	void startRequest();

protected:
	bool eventFilter(QObject *, QEvent *);

private:
	void setFilterModeEnabled(bool enable);
	void setButtons();
	void setLooks(QWidget *w);
	QFont fontForOption(const QString& option);
	void resetWidgets();
	void listAccounts();
	UserListItem* currentUserListItem() const;
	void stopRequest();
	void showProgress(int max);
	void incrementProgress();
	bool selectContact(const QString &accId, const Jid &jid);
	bool selectContact(const QStringList &ids);
	void selectDefaultContact(const QModelIndex &prefer_parent = QModelIndex(), int prefer_row = 0);
	void saveFocus();
	void restoreFocus();
	EDBHandle* getEDBHandle();
	QString getCurrentAccountId() const;
	HistoryContactListModel *contactListModel();

	class Private;
	Private *d;
	Ui::HistoryDlg ui_;
	HistoryContactListModel *_contactListModel;
	QWidget *lastFocus;
	DisplayProxy *displayProxy;
	SearchProxy *searchProxy;
};

#endif
