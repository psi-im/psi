/*
 * historydlg.cpp
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin,
 * Piotr Okonski, Evgeny Khryukin
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

#include <QMessageBox>
#include <QScrollBar>
#include <QMenu>
#include <QProgressDialog>
#include <QKeyEvent>
#include <QTextBlock>

#include "historydlg.h"
#include "psicon.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "coloropt.h"
#include "textutil.h"
#include "jidutil.h"
#include "fileutil.h"
#include "userlist.h"
#include "common.h"

#define SEARCH_PADDING_SIZE 20
#define DISPLAY_PAGE_SIZE   200

static const QString geometryOption = "options.ui.history.size";

static QString getNext(QString *str)
{
	int n = 0;
	// skip leading spaces (but *do* return them later!)
	while(n < (int)str->length() && str->at(n).isSpace()) {
		++n;
	}
	if(n == (int)str->length()) {
		return QString();
	}
	// find end or next space
	while(n < (int)str->length() && !str->at(n).isSpace()) {
		++n;
	}
	QString result = str->mid(0, n);
	*str = str->mid(n);
	return result;
}

// wraps a string against a fixed width
static QStringList wrapString(const QString &str, int wid)
{
	QStringList lines;
	QString cur;
	QString tmp = str;
	//printf("parsing: [%s]\n", tmp.latin1());
	while(1) {
		QString word = getNext(&tmp);
		if(word == QString()) {
			lines += cur;
			break;
		}
		//printf("word:[%s]\n", word.latin1());
		if(!cur.isEmpty()) {
			if((int)cur.length() + (int)word.length() > wid) {
				lines += cur;
				cur = "";
			}
		}
		if(cur.isEmpty()) {
			// trim the whitespace in front
			for(int n = 0; n < (int)word.length(); ++n) {
				if(!word.at(n).isSpace()) {
					if(n > 0) {
						word = word.mid(n);
					}
					break;
				}
			}
		}
		cur += word;
	}
	return lines;
}


SearchProxy::SearchProxy(PsiCon *p, DisplayProxy *d)
	: QObject(0)
	, active(false)
{
	psi = p;
	dp = d;
}

void SearchProxy::find(const QString &str, const QString &acc_id, const Jid &jid, int dir)
{
	if (!active || str != s_string || acc_id != acc_ || jid != jid_)
	{
		active = true;
		s_string = str;
		acc_ = acc_id;
		jid_ = jid;
		direction = dir;
		emit needRequest();
		reqType = ReqFind;
		getEDBHandle()->find(acc_id, str, jid, QDateTime(), EDB::Forward);
		return;
	}

	movePosition(dir);
	if (!dp->moveSearchCursor(dir, 1)) { // tries to move the search cursor into the history widget
		direction = dir;
		emit needRequest();
		reqType = ReqPadding;
		int r_dir = (dir == EDB::Forward) ? EDB::Backward : EDB::Forward;
		getEDBHandle()->get(acc_id, jid, position.date, r_dir, 0, SEARCH_PADDING_SIZE);
	}
}

void SearchProxy::handleResult()
{
	EDBHandle *h = qobject_cast<EDBHandle*>(sender());
	if (!h)
		return;

	const EDBResult r = h->result();
	switch (reqType) {
	case ReqFind:
		handleFoundData(r);
		emit found(total_found);
		break;
	case ReqPadding:
		handlePadding(r);
		break;
	}
	delete h;
}

void SearchProxy::reset()
{
	active = false;
	acc_ = "";
	jid_ = XMPP::Jid();
	s_string = "";
	map.clear();
	list.clear();
	total_found = 0;
	general_pos = 0;
}

EDBHandle *SearchProxy::getEDBHandle()
{
	EDBHandle *h = new EDBHandle(psi->edb());
	connect(h, SIGNAL(finished()), this, SLOT(handleResult()));
	return h;
}

void SearchProxy::movePosition(int dir)
{
	int idx = map.value(position.date.toTime_t(), -1);
	Q_ASSERT(idx >= 0 && idx < list.count());
	if (dir == EDB::Forward)
	{
		if (list.at(idx).num == position.num)
		{
			position.num = 1;
			if (idx == list.size() - 1)
			{
				position.date = list.at(0).date;
				general_pos = 0;
			}
			else
				position.date = list.at(idx + 1).date;
		}
		else
			++position.num;
		++general_pos;
	}
	else
	{
		if (position.num == 1)
		{
			if (idx == 0)
			{
				general_pos = total_found +1;
				position = list.last();
			}
			else
				position = list.at(idx - 1);
		}
		else
			--position.num;
		--general_pos;
	}
}

int SearchProxy::invertSearchPosition(const Position &pos, int dir)
{
	int num = pos.num;
	if (dir == EDB::Backward)
	{
		int idx = map.value(pos.date.toTime_t(), -1);
		Q_ASSERT(idx >= 0 && idx < list.count());
		num = list.at(idx).num - pos.num + 1;
		Q_ASSERT(num > 0);
	}
	return num;
}

void SearchProxy::handleFoundData(const EDBResult &r)
{
	int cnt = r.count();
	if (cnt == 0)
	{
		reset();
		return;
	}

	total_found = 0;
	position.num = 1;
	map.clear();
	list.clear();
	for (int i = 0; i < cnt; ++i)
	{
		EDBItemPtr item = r.value(i);
		PsiEvent::Ptr e(item->event());
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		int m = me->message().body().count(s_string, Qt::CaseInsensitive);
		Position pos;
		pos.date = me->timeStamp();
		uint t = pos.date.toTime_t();
		int idx = map.value(t, -1);
		if (idx == -1)
		{
			pos.num = m;
			map.insert(t, list.size());
			list.append(pos);
		}
		else
			list[idx].num += m;
		if ((direction == EDB::Forward) ? (i == 0) : (i == cnt - 1))
			position.date = pos.date;
		total_found += m;
	}
	position.num = invertSearchPosition(position, direction);
	general_pos = (direction == EDB::Forward) ? 1 : total_found;

	reqType = ReqPadding;
	int r_dir = (direction == EDB::Forward) ? EDB::Backward : EDB::Forward;
	getEDBHandle()->get(acc_, jid_, position.date, r_dir, 0, SEARCH_PADDING_SIZE);
}

void SearchProxy::handlePadding(const EDBResult &r)
{
	int s_pos = invertSearchPosition(position, direction);

	int cnt = r.count();
	if (cnt == 0)
	{
		dp->displayWithSearchCursor(acc_, jid_, position.date, direction, s_string, s_pos);
		return;
	}

	EDBItemPtr item = r.value(cnt - 1);
	PsiEvent::Ptr e(item->event());
	QDateTime s_date = e->timeStamp();
	int idx = map.value(position.date.toTime_t());
	int off = ( direction == EDB::Forward) ? -1 : 1;
	idx += off;
	while (idx >= 0 && idx < list.count())
	{
		const Position &pos = list.at(idx);
		if ((direction == EDB::Forward) ? (pos.date < s_date) : (pos.date > s_date))
			break;
		s_pos += pos.num;
		idx += off;
	}
	dp->displayWithSearchCursor(acc_, jid_, s_date, direction, s_string, s_pos);
}


DisplayProxy::DisplayProxy(PsiCon *p, PsiTextView *v)
	: QObject(0)
{
	psi = p;
	viewWid = v;
	reqType = ReqNone;
	can_backward = false;
	can_forward = false;
	searchParams.searchPos = 0;
	searchParams.cursorPos = -1;
}

void DisplayProxy::displayEarliest(const QString &acc_id, const Jid &jid)
{
	acc_ = acc_id;
	jid_ = jid;
	resetSearch();
	updateQueryParams(EDB::Forward, 0);
	reqType = ReqEarliest;
	getEDBHandle()->get(acc_id, jid, QDateTime(), EDB::Forward, 0, DISPLAY_PAGE_SIZE);
}

void DisplayProxy::displayLatest(const QString &acc_id, const Jid &jid)
{
	acc_ = acc_id;
	jid_ = jid;
	resetSearch();
	updateQueryParams(EDB::Backward, 0);
	reqType = ReqLatest;
	getEDBHandle()->get(acc_id, jid, QDateTime(), EDB::Backward, 0, DISPLAY_PAGE_SIZE);
}

void DisplayProxy::displayFromDate(const QString &acc_id, const Jid &jid, const QDateTime date)
{
	acc_ = acc_id;
	jid_ = jid;
	resetSearch();
	updateQueryParams(EDB::Forward, 0, date);
	reqType = ReqDate;
	getEDBHandle()->get(acc_id, jid, date, EDB::Forward, 0, DISPLAY_PAGE_SIZE);
}

void DisplayProxy::displayNext()
{
	resetSearch();
	updateQueryParams(EDB::Forward, DISPLAY_PAGE_SIZE);
	reqType = ReqNext;
	getEDBHandle()->get(acc_, jid_, queryParams.date, queryParams.direction, queryParams.offset, DISPLAY_PAGE_SIZE);
}

void DisplayProxy::displayPrevious()
{
	resetSearch();
	updateQueryParams(EDB::Backward, DISPLAY_PAGE_SIZE);
	reqType = ReqPrevious;
	getEDBHandle()->get(acc_, jid_, queryParams.date, queryParams.direction, queryParams.offset, DISPLAY_PAGE_SIZE);
}

bool DisplayProxy::moveSearchCursor(int dir, int n)
{
	if (n == 0 || searchParams.searchPos == 0 || searchParams.searchString.isEmpty())
		return false;

	// setting the start cursor position
	QTextCursor t_cursor = viewWid->textCursor();
	bool shiftCursor = false;
	if (searchParams.cursorPos == -1)
	{
		if (dir == EDB::Forward)
			t_cursor.movePosition(QTextCursor::Start);
		else
			t_cursor.movePosition(QTextCursor::End);
	}
	else
	{
		t_cursor.setPosition(searchParams.cursorPos);
		if (dir == EDB::Forward)
			shiftCursor = true;
	}
	viewWid->setTextCursor(t_cursor);

	// settings of the text search options
	QTextDocument::FindFlags find_opt;
	if (dir == EDB::Backward)
		find_opt |= QTextDocument::FindBackward;

	// moving the text cursor
	bool res = true;
	do
	{
		if (shiftCursor)
		{
			t_cursor = viewWid->textCursor();
			t_cursor.movePosition(QTextCursor::Right);
			viewWid->setTextCursor(t_cursor);
		}
		else if (dir == EDB::Forward)
			shiftCursor = true;

		if (!(res = viewWid->find(searchParams.searchString, find_opt)))
			break;
		if (isMessage(viewWid->textCursor()))
			--n;
	} while (n != 0);

	t_cursor = viewWid->textCursor();
	t_cursor.setPosition(t_cursor.selectionStart());
	// save the text cursor position
	searchParams.cursorPos = t_cursor.position();

	if (res)
		emit searchCursorMoved();
	return res;
}

void DisplayProxy::displayWithSearchCursor(const QString &acc_id, const Jid &jid, QDateTime start, int dir, const QString &s_str, int num)
{
	acc_ = acc_id;
	jid_ = jid;

	searchParams.searchDir = dir;
	searchParams.searchPos = num;
	searchParams.cursorPos = -1;
	searchParams.searchString = s_str;

	QDateTime ts = start;
	if (dir == EDB::Backward && !ts.isNull())
		ts = ts.addSecs(1);
	updateQueryParams(dir, 0, ts);

	reqType = ReqDate;
	getEDBHandle()->get(acc_id, jid, queryParams.date, queryParams.direction, 0, DISPLAY_PAGE_SIZE);
}

bool DisplayProxy::isMessage(const QTextCursor &cursor) const
{
	int pos = cursor.positionInBlock();
	if (pos > 22) // the timestamp length
		if (cursor.block().text().leftRef(pos-1).indexOf("> ") != -1) // skip nickname
			return true;
	return false;
}

void DisplayProxy::handleResult()
{
	EDBHandle *h = qobject_cast<EDBHandle*>(sender());
	if (!h)
		return;

	const EDBResult r = h->result();
	can_backward = true;
	can_forward  = true;
	if (r.count() < DISPLAY_PAGE_SIZE)
	{
		switch (reqType) {
		case ReqEarliest:
		case ReqNext:
			can_forward = false;
			break;
		case ReqLatest:
		case ReqPrevious:
			can_backward = false;
			break;
		case ReqDate:
			if (searchParams.searchPos == 0)
				can_forward = false;
			else
			{
				if (searchParams.searchDir == EDB::Forward)
					can_forward = false;
				else
					can_backward = false;
			}
			break;
		default:
			break;
		}
		if (r.count() == 0)
		{
			emit updated();
			delete h;
			return;
		}
	}
	if (queryParams.offset == 0 && queryParams.date.isNull())
	{
		if (queryParams.direction == EDB::Forward)
			can_backward = false;
		else
			can_forward = false;
	}
	switch (reqType) {
	case ReqDate:
		displayResult(r, queryParams.direction);
		moveSearchCursor(searchParams.searchDir, searchParams.searchPos);
		break;
	case ReqEarliest:
		can_backward = false;
		displayResult(r, queryParams.direction);
		break;
	case ReqLatest:
		can_forward = false;
	case ReqNext:
	case ReqPrevious:
		displayResult(r, queryParams.direction);
		break;
	default:
		break;
	}
	delete h;
}

EDBHandle *DisplayProxy::getEDBHandle()
{
	EDBHandle *h = new EDBHandle(psi->edb());
	connect(h, SIGNAL(finished()), this, SLOT(handleResult()));
	return h;
}

void DisplayProxy::resetSearch()
{
	searchParams.searchPos = 0;
	searchParams.cursorPos = -1;
	searchParams.searchString = "";
}

void DisplayProxy::updateQueryParams(int dir, int increase, QDateTime date)
{
	if (increase == 0)
	{
		queryParams.direction = dir;
		queryParams.offset = 0;
		queryParams.date = date;
	}
	else if (queryParams.offset != 0 || dir == queryParams.direction || !queryParams.date.isNull())
	{
		if (dir == queryParams.direction)
			queryParams.offset += increase;
		else
		{
			if (queryParams.offset == 0)
				queryParams.direction = dir;
			else
			{
				Q_ASSERT(queryParams.offset >= increase);
				queryParams.offset -= increase;
			}
		}
	}
}

void DisplayProxy::displayResult(const EDBResult &r, int dir)
{
	viewWid->clear();
	int i, d;
	if (dir == EDB::Forward)
	{
		i = 0;
		d = 1;
	}
	else
	{
		i = r.count() - 1;
		d = -1;
	}

	PsiAccount *acc = NULL;
	if ((psi->edb()->features() & EDB::SeparateAccounts) == 0) {
		acc = psi->contactList()->getAccount(acc_);
		Q_ASSERT(acc);
	}

	bool fAllContacts = jid_.isEmpty();
	while (i >= 0 && i < r.count())
	{
		EDBItemPtr item = r.value(i);
		PsiEvent::Ptr e(item->event());
		if (e->type() == PsiEvent::Message) {
			PsiAccount *pa = (acc) ? acc : e->account();
			QString from = getNick(e->account(), e->from());
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			QString msg = me->message().body();
			msg = TextUtil::linkify(TextUtil::plain2rich(msg));

			if (emoticons)
				msg = TextUtil::emoticonify(msg);
			if (formatting)
				msg = TextUtil::legacyFormat(msg);

			if (me->originLocal())
			{
				QString nick = (pa) ? TextUtil::plain2rich(pa->nick()) : tr("deleted");
				msg = "<span style='color:" + sentColor + "'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]") + " &lt;" + nick
					+ ((fAllContacts) ? QString(" -> %1").arg(TextUtil::plain2rich(from)) : QString())
					+ "&gt; " + msg + "</span>";
			}
			else
				msg = "<span style='color:" + receivedColor + "'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]") + " &lt;"
					+  TextUtil::plain2rich(from) + "&gt; " + msg + "</span>";

			viewWid->appendText(msg);
		}
		i += d;
	}
	viewWid->verticalScrollBar()->setValue(viewWid->verticalScrollBar()->maximum());
	emit updated();
}

QString DisplayProxy::getNick(PsiAccount *pa, const Jid &jid) const
{
	QString from;
	if (pa)
	{
		UserListItem *u = pa->findFirstRelevant(jid);
		if (u && !u->name().trimmed().isEmpty())
			from = u->name().trimmed();
	}
	if (from.isEmpty())
	{
		from = jid.resource().trimmed(); // for offline conferences
		if (from.isEmpty())
			from = jid.node();
	}
	return from;
}


class HistoryDlg::Private
{
public:
	Jid jid;
	PsiAccount *pa;
	PsiCon *psi;
#ifndef HAVE_X11
	bool autoCopyText;
#endif
};

HistoryDlg::HistoryDlg(const Jid &jid, PsiAccount *pa)
	: AdvancedWidget<QDialog>(0, Qt::Window)
	, lastFocus(NULL)
{
	ui_.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(false);
	d = new Private;
	d->psi = pa->psi();
	d->jid = jid;
	d->pa = pa;
	pa->dialogRegister(this, d->jid);

	displayProxy = new DisplayProxy(pa->psi(), ui_.msgLog);
	searchProxy = new SearchProxy(pa->psi(), displayProxy);
	connect(displayProxy, SIGNAL(updated()), this, SLOT(viewUpdated()));
	connect(displayProxy, SIGNAL(searchCursorMoved()), this, SLOT(updateSearchHint()));
	connect(searchProxy, SIGNAL(found(int)), this, SLOT(showFoundResult(int)));
	connect(searchProxy, SIGNAL(needRequest()), this, SLOT(startRequest()));

	//workaround calendar size
	int minWidth = ui_.calendar->minimumSizeHint().width();
	if(minWidth > ui_.calendar->maximumWidth()) {
		ui_.calendar->setMaximumWidth(minWidth);
		ui_.contactList->setMaximumWidth(minWidth);
		ui_.contactFilterEdit->setMaximumWidth(minWidth);
		ui_.accountsBox->setMaximumWidth(minWidth);
		ui_.buttonRefresh->setMaximumWidth(minWidth);
	}

#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/history").icon());
#endif
	ui_.tbFindForward->setIcon(IconsetFactory::icon("psi/arrowDown").icon());
	ui_.tbFindBackward->setIcon(IconsetFactory::icon("psi/arrowUp").icon());

	ui_.msgLog->setFont(fontForOption("options.ui.look.font.chat"));
	setLooks(ui_.msgLog);
	ui_.contactList->setFont(fontForOption("options.ui.look.font.contactlist"));

	ui_.calendar->setFirstDayOfWeek(firstDayOfWeekFromLocale());

	connect(ui_.searchField, SIGNAL(returnPressed()), SLOT(findMessages()));
	connect(ui_.searchField, SIGNAL(textChanged(const QString)), SLOT(highlightBlocks()));
	connect(ui_.buttonPrevious, SIGNAL(released()), SLOT(getPrevious()));
	connect(ui_.buttonNext, SIGNAL(released()), SLOT(getNext()));
	connect(ui_.buttonRefresh, SIGNAL(released()), SLOT(refresh()));
	connect(ui_.contactList, SIGNAL(clicked(QModelIndex)), SLOT(openSelectedContact()));
	connect(ui_.tbFindForward, SIGNAL(clicked()), SLOT(findMessages()));
	connect(ui_.tbFindBackward, SIGNAL(clicked()), SLOT(findMessages()));
	connect(ui_.buttonLastest, SIGNAL(released()), SLOT(getLatest()));
	connect(ui_.buttonEarliest, SIGNAL(released()), SLOT(getEarliest()));
	connect(ui_.calendar, SIGNAL(selectionChanged()), SLOT(getDate()));
	connect(ui_.calendar, SIGNAL(activated(QDate)), SLOT(getDate()));
#ifndef HAVE_X11	// linux has this feature built-in
	optionUpdated("options.ui.automatically-copy-selected-text");
	connect(ui_.msgLog, SIGNAL(selectionChanged()), SLOT(autoCopy()));
	connect(ui_.msgLog, SIGNAL(cursorPositionChanged()), SLOT(autoCopy()));
#endif
	optionUpdated("options.ui.look.colors.messages.sent");
	optionUpdated("options.ui.look.colors.messages.received");
	optionUpdated("options.ui.emoticons.use-emoticons");
	optionUpdated("options.ui.chat.legacy-formatting");
	connect(PsiOptions::instance(), SIGNAL(optionChanged(QString)), SLOT(optionUpdated(QString)));

	connect(pa, SIGNAL(removedContact(PsiContact*)), SLOT(removedContact(PsiContact*)));

	listAccounts();

	ui_.contactList->installEventFilter(this);
	_contactListModel = new HistoryContactListModel(this);
	QSortFilterProxyModel *proxy = new HistoryContactListProxyModel(this);
	proxy->setSourceModel(_contactListModel);
	ui_.contactList->setModel(proxy);
	{
		int f = d->psi->edb()->features();
		_contactListModel->displayPrivateContacts((f & EDB::PrivateContacts) != 0);
		_contactListModel->displayAllContacts((f & EDB::AllContacts) != 0);
	}
	_contactListModel->updateContacts(pa->psi(), getCurrentAccountId());
	selectContact(d->pa->id(), d->jid);
	openSelectedContact();
	connect(proxy, SIGNAL(layoutChanged()), ui_.contactList, SLOT(expandAll()));

	ui_.contactFilterEdit->setVisible(false);
	ui_.contactFilterEdit->installEventFilter(this);
	connect(ui_.contactFilterEdit, SIGNAL(textChanged(QString)), proxy, SLOT(setFilterFixedString(QString)));

	setGeometryOptionPath(geometryOption);
}

HistoryDlg::~HistoryDlg()
{
	delete d;
	delete searchProxy;
	delete displayProxy;
}

bool HistoryDlg::eventFilter(QObject *obj, QEvent *e)
{
	if (e->type() == QEvent::ContextMenu)
	{
		if(obj == ui_.contactList)
		{
			e->accept();
			QTimer::singleShot(0, this, SLOT(doMenu()));
			return true;
		}
	}
	else if (e->type() == QEvent::KeyPress)
	{
		QKeyEvent *ke = static_cast<QKeyEvent*>(e);
		if (ke->key() == Qt::Key_Escape)
		{
			if (obj == ui_.contactList || obj == ui_.contactFilterEdit)
			{
				setFilterModeEnabled(false);
				ui_.contactFilterEdit->setText("");
				const QString paId = ui_.accountsBox->itemData(ui_.accountsBox->currentIndex()).toString();
				selectContact(paId, d->jid);
				return true;
			}
		}
		else if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return)
		{
			if (obj == ui_.contactFilterEdit)
			{
				ui_.contactList->setFocus();
				return true;
			}
			if (obj == ui_.contactList)
			{
				openSelectedContact();
				return true;
			}
		}
		else if (obj == ui_.contactList)
		{
			if ((ke->modifiers() & ~Qt::ShiftModifier) == 0)
			{
				QString text = ke->text();
				if (!text.isEmpty() && (text[0].isLetterOrNumber() || text[0].isPrint()))
				{
					setFilterModeEnabled(true);
					ui_.contactFilterEdit->setText(text);
					return true;
				}
			}
		}
	}

	return QDialog::eventFilter(obj, e);
}

void HistoryDlg::setFilterModeEnabled(bool enable)
{
	ui_.contactFilterEdit->setVisible(enable);
	if (enable)
		ui_.contactFilterEdit->setFocus();
}

QFont HistoryDlg::fontForOption(const QString &option)
{
	QFont font;
	font.fromString(PsiOptions::instance()->getOption(option).toString());
	return font;
}

void HistoryDlg::changeAccount(const QString /*accountName*/)
{
	resetWidgets();
	d->jid = QString();
	QString paId = ui_.accountsBox->itemData(ui_.accountsBox->currentIndex()).toString();
	contactListModel()->updateContacts(d->psi, paId);
	selectDefaultContact();
	openSelectedContact();
}

bool HistoryDlg::selectContact(const QString &accId, const Jid &jid)
{
	QStringList ids(accId + "|" + jid.full());
	if (!jid.resource().isEmpty())
		ids.append(accId + "|" + jid.bare());
	return selectContact(ids);
}

bool HistoryDlg::selectContact(const QStringList &ids)
{
	QAbstractItemModel *model = ui_.contactList->model();
	QModelIndex startIndex = model->index(0, 0);
	foreach (const QString &id, ids)
	{
		QModelIndexList ilist = model->match(startIndex, HistoryContactListModel::ItemIdRole, id, -1, Qt::MatchRecursive | Qt::MatchFixedString);
		if (ilist.count() > 0)
		{
			ui_.contactList->selectionModel()->setCurrentIndex(ilist.at(0), QItemSelectionModel::SelectCurrent);
			return true;
		}
	}
	return false;
}

void HistoryDlg::selectDefaultContact(const QModelIndex &prefer_parent, int prefer_row)
{
	QList<QModelIndex>ilist;
	ilist.append(prefer_parent);
	ilist.append(ui_.contactList->model()->index(0, 0));
	foreach (const QModelIndex &parent, ilist)
	{
		if (parent.isValid())
		{
			for (int i = 1; i <= 2; ++i)
			{
				QModelIndex index = parent.child(prefer_row, 0);
				if (index.isValid())
				{
					ui_.contactList->selectionModel()->clearSelection();
					ui_.contactList->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);
					return;
				}
				prefer_row = 0;
			}
		}
	}
}

void HistoryDlg::saveFocus()
{
	lastFocus = ui_.contactList->hasFocus() ? ui_.contactList : NULL;
}

void HistoryDlg::restoreFocus()
{
	if (lastFocus)
		lastFocus->setFocus();
	else
		setFocus();
}

void HistoryDlg::resetWidgets()
{
	ui_.msgLog->clear();
	ui_.searchField->clear();
	ui_.searchResult->setVisible(false);
	ui_.searchResult->clear();
}

void HistoryDlg::listAccounts()
{
	if ((d->psi->edb()->features() & EDB::AllAccounts) != 0)
		ui_.accountsBox->addItem(IconsetFactory::icon("psi/account").icon(), tr("All accounts"), QVariant());
	if (d->psi)
	{
		foreach (PsiAccount* account, d->psi->contactList()->enabledAccounts())
			ui_.accountsBox->addItem(IconsetFactory::icon("psi/account").icon(), account->nameWithJid(), QVariant(account->id()));
	}
	//select active account
	ui_.accountsBox->setCurrentIndex(ui_.accountsBox->findData(getCurrentAccountId()));
	//connect signal after the list is populated to prevent execution in the middle of the loop
	connect(ui_.accountsBox, SIGNAL(currentIndexChanged(const QString)), SLOT(changeAccount(const QString)));
}

void HistoryDlg::openSelectedContact()
{
	setFilterModeEnabled(false);
	ui_.contactFilterEdit->setText("");
	QModelIndex index = ui_.contactList->selectionModel()->currentIndex();
	if (index.isValid() && index.data(HistoryContactListModel::ItemTypeRole) != HistoryContactListModel::Group)
	{
		QString id = ui_.contactList->model()->data(index, HistoryContactListModel::ItemIdRole).toString();
		if (!id.isEmpty())
		{
			resetWidgets();
			QString sTitle;
			if (id != "*all")
			{
				d->pa = d->psi->contactList()->getAccount(id.section('|', 0, 0));
				d->jid = XMPP::Jid(id.section('|', 1));
				UserListItem *u = currentUserListItem();
				if (u)
					sTitle = u->name() + " (" + u->jid().full() + ")";
				else
					sTitle = d->jid.full();
			}
			else
			{
				d->pa = NULL;
				d->jid = Jid();
				QString paId = ui_.accountsBox->itemData(ui_.accountsBox->currentIndex()).toString();
				if (!paId.isEmpty())
					d->pa = d->psi->contactList()->getAccount(paId);
				sTitle = tr("All contacts");
			}
			setWindowTitle(sTitle);
			getLatest();
		}
		ui_.contactList->scrollTo(index);
	}
}

void HistoryDlg::highlightBlocks()
{
	const QString text = ui_.searchField->text();
	QTextCursor cur = ui_.msgLog->textCursor();
    QTextCursor old = cur;
    int sc_pos = 0;
    if (ui_.msgLog->verticalScrollBar())
        sc_pos = ui_.msgLog->verticalScrollBar()->value();
	cur.clearSelection();
	cur.movePosition(QTextCursor::Start);
	ui_.msgLog->setTextCursor(cur);

	QList<QTextEdit::ExtraSelection> extras;
	QTextEdit::ExtraSelection highlight;
	highlight.format.setBackground(Qt::yellow);
	highlight.cursor = ui_.msgLog->textCursor();

	bool found = ui_.msgLog->find(text);
	while (found)
	{
		highlight.cursor = ui_.msgLog->textCursor();
		if (displayProxy->isMessage(highlight.cursor))
			extras << highlight;
		found = ui_.msgLog->find(text);
	}

	ui_.msgLog->setExtraSelections(extras);
    ui_.msgLog->setTextCursor(old);
    if (ui_.msgLog->verticalScrollBar())
        ui_.msgLog->verticalScrollBar()->setValue(sc_pos);
}

void HistoryDlg::findMessages()
{
	const QString str = ui_.searchField->text();
	if (str.isEmpty())
		return;

	int dir = (sender() != ui_.tbFindBackward) ? EDB::Forward : EDB::Backward;
	searchProxy->find(str, getCurrentAccountId(), d->jid, dir);
}

void HistoryDlg::removeHistory()
{
	int res = QMessageBox::question(this, tr("Remove history"),
					tr("Are you sure you want to completely remove history for a contact %1?").arg(d->jid.full())
					,QMessageBox::Ok | QMessageBox::Cancel);
	if(res == QMessageBox::Ok) {
		getEDBHandle()->erase(getCurrentAccountId(), d->jid);
		QModelIndex i_index = ui_.contactList->selectionModel()->currentIndex();
		QModelIndex p_index = i_index.parent();
		QString p_id = p_index.data(HistoryContactListModel::ItemIdRole).toString();
		if (p_id == "not-in-list" || p_id == "conf-private") {
			ui_.contactList->model()->removeRow(i_index.row(), p_index);
			selectDefaultContact(p_index, i_index.row());
		}
		openSelectedContact();
	}
}

void HistoryDlg::openChat()
{
	if (d->pa && !d->jid.isEmpty())
		d->pa->actionOpenChat2(d->jid);
}

void HistoryDlg::exportHistory()
{
	QString them;
	UserListItem *u = currentUserListItem();
	if (u)
		them = JIDUtil::nickOrJid(u->name(), u->jid().full());
	else
		them = d->jid.full();
	QString s = (!them.isEmpty()) ? JIDUtil::encode(them).toLower() : "all_contacts";
	QString fname = FileUtil::getSaveFileName(this,
						  tr("Export message history"),
						  s + ".txt",
						  tr("Text files (*.txt);;All files (*.*)"));
	if(fname.isEmpty())
		return;

	QFile f(fname);
	if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QMessageBox::information(this, tr("Error"), tr("Error writing to file."));
		return;
	}
	QTextStream stream(&f);

	EDB *edb = NULL;
	QString us;
	//PsiAccount *pa = NULL;
	if (d->pa) {
		edb = d->pa->edb();
		us  = d->pa->nick();
	} else {
		edb = d->psi->edb();
		us  = d->psi->contactList()->defaultAccount()->nick();
	}

	EDBHandle *h;
	int start = 0;
	startRequest();
	QString paId = getCurrentAccountId();
	int max = 0;
	{
		quint64 edbCnt = d->psi->edb()->eventsCount(paId, d->jid);
		if (edbCnt > 1000) {
			max = edbCnt / 1000;
			if ((edbCnt % 1000) != 0)
				++max;
			showProgress(max);
		}
	}
	PsiAccount *acc = NULL;
	if ((d->psi->edb()->features() & EDB::SeparateAccounts) == 0) {
		Q_ASSERT(d->pa);
		acc = d->pa;
	}
	while(1) {
		h = new EDBHandle(edb);
		h->get(paId, d->jid, QDateTime(), EDB::Forward, start, 1000);
		while(h->busy()) {
			qApp->processEvents();
		}

		const EDBResult r = h->result();
		int cnt = r.count();

		// events are in forward order
		for(int i = 0; i < cnt; ++i) {
			EDBItemPtr item = r.value(i);
			PsiEvent::Ptr e(item->event());
			QString txt;

			QString ts = e->timeStamp().toString(Qt::LocalDate);

			QString nick;
			if (e->originLocal()) {
				if (acc)
					nick = acc->nick();
				else if (e->account())
					nick = e->account()->nick();
				else
					nick = tr("deleted");
			}
			else {
				if (!them.isEmpty())
					nick = them;
				else
					nick = e->from().full();
			}

			if(e->type() == PsiEvent::Message) {
				MessageEvent::Ptr me = e.staticCast<MessageEvent>();
				stream << QString("[%1] <%2>: ").arg(ts, nick)/* << endl*/;

				QStringList lines = me->message().body().split('\n', QString::KeepEmptyParts);
				foreach(const QString& str, lines) {
					QStringList sub = wrapString(str, 72);
					foreach(const QString& str2, sub) {
						txt += str2 + "\n" + QString("    ");
					}
				}
			}
			else {
				continue;
			}

			stream << txt << endl;
		}
		delete h;

		if (max > 0)
			incrementProgress();

		// done!
		if(cnt == 0)
			break;

		start += 1000;
	}
	f.close();
	stopRequest();
}

void HistoryDlg::doMenu()
{
	QModelIndex index = ui_.contactList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;
	int itype = index.data(HistoryContactListModel::ItemTypeRole).toInt();
	if (itype != HistoryContactListModel::RosterContact && itype != HistoryContactListModel::NotInRosterContact)
		return;

	openSelectedContact();
	QMenu *m = new QMenu();
	QAction *chat = m->addAction(IconsetFactory::icon("psi/chat").icon(), tr("&Open chat"), this, SLOT(openChat()));
	QAction *exp  = m->addAction(IconsetFactory::icon("psi/save").icon(), tr("&Export history"), this, SLOT(exportHistory()));
	QAction *del  =  m->addAction(IconsetFactory::icon("psi/clearChat").icon(), tr("&Delete history"), this, SLOT(removeHistory()));

	if (!d->pa || d->jid.isEmpty()) {
		chat->setEnabled(false);
		del->setEnabled(false);
	}
	else if (!d->jid.resource().isEmpty()) // Private messages in conferences
		chat->setEnabled(false);
	int features = d->psi->edb()->features();
	if ((!d->pa && !(features & EDB::AllAccounts)) || (d->jid.isEmpty() && !(features & EDB::AllContacts)))
		exp->setEnabled(false);

	m->exec(QCursor::pos());
	delete m;
}

void HistoryDlg::edbFinished()
{
	stopRequest();

	EDBHandle* h = qobject_cast<EDBHandle*>(sender());
	delete h;
}

void HistoryDlg::setButtons()
{
	bool can_backward = displayProxy->canBackward();
	bool can_forward  = displayProxy->canForward();
	ui_.buttonPrevious->setEnabled(can_backward);
	ui_.buttonNext->setEnabled(can_forward);
	ui_.buttonEarliest->setEnabled(can_backward);
	ui_.buttonLastest->setEnabled(can_forward);
}

void HistoryDlg::setLooks(QWidget *w)
{
	QPalette pal = w->palette();
	pal.setColor(QPalette::Inactive, QPalette::HighlightedText,
			pal.color(QPalette::Active, QPalette::HighlightedText));
	pal.setColor(QPalette::Inactive, QPalette::Highlight,
			pal.color(QPalette::Active, QPalette::Highlight));
	w->setPalette(pal);
}

void HistoryDlg::refresh()
{
	ui_.calendar->setSelectedDate(QDate::currentDate());
	ui_.searchField->clear();
	getLatest();
}

void HistoryDlg::getLatest()
{
	startRequest();
	displayProxy->displayLatest(getCurrentAccountId(), d->jid);
}

void HistoryDlg::getEarliest()
{
	startRequest();
	displayProxy->displayEarliest(getCurrentAccountId(), d->jid);
}

void HistoryDlg::getPrevious()
{
	startRequest();
	displayProxy->displayPrevious();
}

void HistoryDlg::getNext()
{
	startRequest();
	displayProxy->displayNext();
}

void HistoryDlg::getDate()
{
	QDateTime ts(ui_.calendar->selectedDate());
	startRequest();
	displayProxy->displayFromDate(getCurrentAccountId(), d->jid, ts);
}

void HistoryDlg::removedContact(PsiContact *pc)
{
	Q_UNUSED(pc);

	QString cid;
	QModelIndex index = ui_.contactList->selectionModel()->currentIndex();
	if (index.isValid())
		cid = ui_.contactList->model()->data(index, HistoryContactListModel::ItemIdRole).toString();

	contactListModel()->updateContacts(d->psi, getCurrentAccountId());

	if (!cid.isEmpty() && !selectContact(QStringList(cid)))
		resetWidgets();
}

void HistoryDlg::optionUpdated(const QString &option)
{
#ifndef HAVE_X11
	if(option == "options.ui.automatically-copy-selected-text") {
		d->autoCopyText = PsiOptions::instance()->getOption(option).toBool();
	} else
#endif
	if(option == "options.ui.look.colors.messages.sent")
			displayProxy->setSentColor(PsiOptions::instance()->getOption(option).toString());
	else if(option == "options.ui.look.colors.messages.received")
			displayProxy->setReceivedColor(PsiOptions::instance()->getOption(option).toString());
	else if(option == "options.ui.emoticons.use-emoticons")
			displayProxy->setEmoticonsFlag(PsiOptions::instance()->getOption(option).toBool());
	else if(option == "options.ui.chat.legacy-formatting")
			displayProxy->setFormattingFlag(PsiOptions::instance()->getOption(option).toBool());
}

#ifndef HAVE_X11
void HistoryDlg::autoCopy()
{
	if (d->autoCopyText) {
		ui_.msgLog->copy();
	}
}
#endif

void HistoryDlg::viewUpdated()
{
	highlightBlocks();
	setButtons();
	stopRequest();
}

void HistoryDlg::showFoundResult(int rows)
{
	if (rows == 0)
		stopRequest();
	if (searchProxy->totalFound() == 0)
		updateSearchHint();
}

void HistoryDlg::updateSearchHint()
{
	int cnt = searchProxy->totalFound();
	if (cnt > 0)
		ui_.searchResult->setText(tr("%1 of %2 matches").arg(searchProxy->cursorPosition()).arg(cnt));
	else
		ui_.searchResult->setText(tr("No matches were found"));
	ui_.searchResult->setVisible(true);
}

UserListItem* HistoryDlg::currentUserListItem() const
{
	UserListItem* u = 0;
	if (d->pa && !d->jid.isEmpty())
		u = d->pa->findFirstRelevant(d->jid);
	return u;
}

void HistoryDlg::startRequest()
{
	if(!ui_.busy->isActive()) {
		ui_.busy->start();
	}
	saveFocus();
	setEnabled(false);
	qApp->processEvents();
}

void HistoryDlg::stopRequest()
{
	if(ui_.busy->isActive()) {
		ui_.busy->stop();
	}
	ui_.progressBar->setVisible(false);
	setEnabled(true);
	restoreFocus();
}

void HistoryDlg::showProgress(int max)
{
	ui_.progressBar->setValue(0);
	ui_.progressBar->setMaximum(max);
	ui_.progressBar->setVisible(true);
}

void HistoryDlg::incrementProgress()
{
	ui_.progressBar->setValue(ui_.progressBar->value() + 1);
}

EDBHandle* HistoryDlg::getEDBHandle()
{
	EDBHandle* h = new EDBHandle(d->psi->edb());
	connect(h, SIGNAL(finished()), SLOT(edbFinished()));
	return h;
}

QString HistoryDlg::getCurrentAccountId() const
{
	if (d->pa)
		return d->pa->id();
	return QString();
}

HistoryContactListModel *HistoryDlg::contactListModel()
{
	return _contactListModel;
}
