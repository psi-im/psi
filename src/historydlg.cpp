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

static const QString geometryOption = "options.ui.history.size";

static QString getNext(QString *str)
{
	int n = 0;
	// skip leading spaces (but *do* return them later!)
	while(n < (int)str->length() && str->at(n).isSpace()) {
		++n;
	}
	if(n == (int)str->length()) {
		return QString::null;
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
		if(word == QString::null) {
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


class HistoryDlg::Private
{
public:
	Jid jid;
	PsiAccount *pa;
	PsiCon *psi;
	QString id_prev, id_begin, id_end, id_next;
	HistoryDlg::RequestType reqType;
	QString findStr;
	QDate date;
#ifndef HAVE_X11
	bool autoCopyText;
#endif
	bool formatting;
	bool emoticons;
	QString sentColor;
	QString receivedColor;
};

HistoryDlg::HistoryDlg(const Jid &jid, PsiAccount *pa)
	: AdvancedWidget<QDialog>(0, Qt::Window)
	, lastFocus(NULL)
{
	ui_.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(false);
	d = new Private;
	d->reqType = TypeNone;
	d->pa = pa;
	d->psi = pa->psi();
	d->jid = jid;
	d->pa->dialogRegister(this, d->jid);

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
	ui_.tb_find->setIcon(IconsetFactory::icon("psi/search").icon());

	ui_.msgLog->setFont(fontForOption("options.ui.look.font.chat"));
	ui_.contactList->setFont(fontForOption("options.ui.look.font.contactlist"));

	ui_.calendar->setFirstDayOfWeek(firstDayOfWeekFromLocale());

	connect(ui_.searchField, SIGNAL(returnPressed()), SLOT(findMessages()));
	connect(ui_.searchField, SIGNAL(textChanged(const QString)), SLOT(highlightBlocks(const QString)));
	connect(ui_.buttonPrevious, SIGNAL(released()), SLOT(getPrevious()));
	connect(ui_.buttonNext, SIGNAL(released()), SLOT(getNext()));
	connect(ui_.buttonRefresh, SIGNAL(released()), SLOT(refresh()));
	connect(ui_.contactList, SIGNAL(clicked(QModelIndex)), SLOT(openSelectedContact()));
	connect(ui_.tb_find, SIGNAL(clicked()), SLOT(findMessages()));
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

	connect(d->pa, SIGNAL(removedContact(PsiContact*)), SLOT(removedContact(PsiContact*)));

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
	ui_.msgLog->clear();
	setButtons(false);
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

void HistoryDlg::listAccounts()
{
	if (d->psi)
	{
		foreach (PsiAccount* account, d->psi->contactList()->enabledAccounts())
			ui_.accountsBox->addItem(IconsetFactory::icon("psi/account").icon(), account->nameWithJid(), QVariant(account->id()));
	}
	//select active account
	ui_.accountsBox->setCurrentIndex(ui_.accountsBox->findData(d->pa->id()));
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
			ui_.msgLog->clear();
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

void HistoryDlg::highlightBlocks(const QString text)
{
	QTextCursor cur = ui_.msgLog->textCursor();
	cur.clearSelection();
	cur.movePosition(QTextCursor::Start);
	ui_.msgLog->setTextCursor(cur);

	if (text.isEmpty()) {
		getLatest();
		return;
	}

	QList<QTextEdit::ExtraSelection> extras;
	QTextEdit::ExtraSelection highlight;
	highlight.format.setBackground(Qt::yellow);
	highlight.cursor = ui_.msgLog->textCursor();

	bool found = ui_.msgLog->find(text);
	while (found)
	{
		highlight.cursor = ui_.msgLog->textCursor();
		extras << highlight;
		found = ui_.msgLog->find(text);
	}

	ui_.msgLog->setExtraSelections(extras);
}

void HistoryDlg::findMessages()
{
	//get the oldest event as a starting point
	startRequest();
	d->reqType = TypeFindOldest;
	getEDBHandle()->getOldest(d->jid, 1);
}

void HistoryDlg::removeHistory()
{
	int res = QMessageBox::question(this, tr("Remove history"),
					tr("Are you sure you want to completely remove history for a contact %1?").arg(d->jid.bare())
					,QMessageBox::Ok | QMessageBox::Cancel);
	if(res == QMessageBox::Ok) {
		getEDBHandle()->erase(d->jid);
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
	QString s = JIDUtil::encode(them).toLower();
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

	QString id;
	EDBHandle *h;
	startRequest();
	while(1) {
		h = new EDBHandle(edb);
		if(id.isEmpty()) {
			h->getOldest(d->jid, 1000);
		}
		else {
			h->get(d->jid, id, EDB::Forward, 1000);
		}
		while(h->busy()) {
			qApp->processEvents();
		}

		const EDBResult r = h->result();
		int cnt = r.count();

		// events are in forward order
		for(int i = 0; i < cnt; ++i) {
			EDBItemPtr item = r.value(i);
			id = item->nextId();
			PsiEvent::Ptr e(item->event());
			QString txt;

			QString ts = e->timeStamp().toString(Qt::LocalDate);

			QString nick;
			if(e->originLocal())
				nick = us;
			else if (them.isEmpty())
				nick = e->from().full();
			else
				nick = them;

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

		// done!
		if(cnt == 0 || id.isEmpty()) {
			break;
		}
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
	setButtons(false);
	stopRequest();

	EDBHandle* h = qobject_cast<EDBHandle*>(sender());
	if(!h) {
		return;
	}

	const EDBResult r = h->result();
	if (h->lastRequestType() == EDBHandle::Read)
	{
		if (r.count() > 0)
		{
			if (d->reqType == TypeLatest || d->reqType == TypePrevious)
			{
				// events are in backward order
				// first entry is the end event
				EDBItemPtr it = r.first();
				d->id_end = it->id();
				d->id_next = it->nextId();
				// last entry is the begin event
				it = r.last();
				d->id_begin = it->id();
				d->id_prev = it->prevId();
				displayResult(r, EDB::Forward);
				setButtons();
			}
			else if (d->reqType == TypeEarliest || d->reqType == TypeNext || d->reqType == TypeDate)
			{
				// events are in forward order
				// last entry is the end event
				EDBItemPtr it = r.last();
				d->id_end = it->id();
				d->id_next = it->nextId();
				// first entry is the begin event
				it = r.first();
				d->id_begin = it->id();
				d->id_prev = it->prevId();
				displayResult(r, EDB::Backward);
				setButtons();
			}
			else if (d->reqType == TypeFindOldest)
			{
				QString str = ui_.searchField->text();
				if (str.isEmpty())
				{
					getLatest();
				}
				else
				{
					d->reqType = TypeFind;
					d->findStr = str;
					EDBItemPtr ei = r.first();
					startRequest();
					getEDBHandle()->find(str, d->jid, ei->id(), EDB::Forward);
					setButtons();
				}
			}
			else if (d->reqType == TypeFind)
			{
				displayResult(r, EDB::Forward);
				highlightBlocks(ui_.searchField->text());
			}

		}
		else
		{
			ui_.msgLog->clear();
		}
	}
	delete h;
}

void HistoryDlg::setButtons()
{
	ui_.buttonPrevious->setEnabled(!d->id_prev.isEmpty());
	ui_.buttonNext->setEnabled(!d->id_next.isEmpty());
	ui_.buttonEarliest->setEnabled(!d->id_prev.isEmpty());
	ui_.buttonLastest->setEnabled(!d->id_next.isEmpty());
}

void HistoryDlg::setButtons(bool act)
{
	ui_.buttonPrevious->setEnabled(act);
	ui_.buttonNext->setEnabled(act);
	ui_.buttonEarliest->setEnabled(act);
	ui_.buttonLastest->setEnabled(act);
}

void HistoryDlg::refresh()
{
	ui_.calendar->setSelectedDate(QDate::currentDate());
	ui_.searchField->clear();
	getLatest();
}

void HistoryDlg::getLatest()
{
	d->reqType = TypeLatest;
	startRequest();
	getEDBHandle()->getLatest(d->jid, 50);
}

void HistoryDlg::getEarliest()
{
	d->reqType = TypeEarliest;
	startRequest();
	getEDBHandle()->getOldest(d->jid, 50);
}

void HistoryDlg::getPrevious()
{
	d->reqType = TypePrevious;
	ui_.buttonPrevious->setEnabled(false);
	getEDBHandle()->get(d->jid, d->id_prev, EDB::Backward, 50);
}

void HistoryDlg::getNext()
{
	d->reqType = TypeNext;
	ui_.buttonNext->setEnabled(false);
	getEDBHandle()->get(d->jid, d->id_next, EDB::Forward, 50);
}

void HistoryDlg::getDate()
{
	const QDate date = ui_.calendar->selectedDate();
	d->reqType = TypeDate;
	d->date = date;
	QDateTime first (d->date);
	QDateTime last = first.addDays(1);
	startRequest();
	getEDBHandle()->getByDate(d->jid, first, last);
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
		ui_.msgLog->clear();
}

void HistoryDlg::optionUpdated(const QString &option)
{
#ifndef HAVE_X11
	if(option == "options.ui.automatically-copy-selected-text") {
		d->autoCopyText = PsiOptions::instance()->getOption(option).toBool();
	} else
#endif
	if(option == "options.ui.look.colors.messages.sent")
			d->sentColor = PsiOptions::instance()->getOption(option).toString();
	else if(option == "options.ui.look.colors.messages.received")
			d->receivedColor = PsiOptions::instance()->getOption(option).toString();
	else if(option == "options.ui.emoticons.use-emoticons")
			d->emoticons  = PsiOptions::instance()->getOption(option).toBool();
	else if(option == "options.ui.chat.legacy-formatting")
			d->formatting = PsiOptions::instance()->getOption(option).toBool();
}
#ifndef HAVE_X11
void HistoryDlg::autoCopy()
{
	if (d->autoCopyText) {
		ui_.msgLog->copy();
	}
}
#endif
void HistoryDlg::displayResult(const EDBResult r, int direction, int max)
{
	int i  = (direction == EDB::Forward) ? r.count() - 1 : 0;
	int at = 0;
	ui_.msgLog->clear();
	PsiAccount *pa = d->pa ? d->pa : d->psi->contactList()->defaultAccount();
	QString nick = TextUtil::plain2rich(pa->nick());
	while (i >= 0 && i <= r.count() - 1 && (max == -1 ? true : at < max))
	{
		EDBItemPtr item = r.value(i);
		PsiEvent::Ptr e(item->event());
		if (e->type() == PsiEvent::Message)
		{
			QString from = getNick(e->account(), e->from());
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			QString msg = me->message().body();
			msg = TextUtil::linkify(TextUtil::plain2rich(msg));

			if (d->emoticons)
				msg = TextUtil::emoticonify(msg);
			if (d->formatting)
				msg = TextUtil::legacyFormat(msg);

			if (me->originLocal())
				msg = "<span style='color:"+d->sentColor+"'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]")+" &lt;"+ nick +"&gt; " + msg + "</span>";
			else
				msg = "<span style='color:"+d->receivedColor+"'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]") + " &lt;" +  TextUtil::plain2rich(from) + "&gt; " + msg + "</span>";

			ui_.msgLog->appendText(msg);
		}

		++at;
		i += (direction == EDB::Forward) ? -1 : +1;
	}

	ui_.msgLog->verticalScrollBar()->setValue(ui_.msgLog->verticalScrollBar()->maximum());
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
}

void HistoryDlg::stopRequest()
{
	if(ui_.busy->isActive()) {
		ui_.busy->stop();
	}
	setEnabled(true);
	restoreFocus();
#ifdef Q_OS_MAC
	// To workaround a Qt bug
	// https://bugreports.qt-project.org/browse/QTBUG-26351
	setFocus();
#endif
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

QString HistoryDlg::getNick(PsiAccount *pa, const Jid &jid) const
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
