/*
 * historydlg.cpp
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#include "historydlg.h"
#include <QtGui>
#include <QDir>
#include "psiaccount.h"
#include "psievent.h"
#include "busywidget.h"
#include "applicationinfo.h"
#include "common.h"
#include "psicon.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "psiiconset.h"
#include "textutil.h"
#include "jidutil.h"
#include "xmpp_jid.h"
#include "userlist.h"
#include "psioptions.h"
#include "msgmle.h"
#include "chatdlg.h"
class HistoryDlg::Private
{
public:
        Private() {}

        Jid jid;
        PsiAccount *pa;
        PsiCon *psi;
        EDBHandle *h;
        QString id_prev, id_begin, id_end, id_next;
        int reqtype;
        QString findStr;
};

HistoryDlg::HistoryDlg(const Jid &jid, PsiAccount *pa)
{
	ui_.setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);
        d = new Private;
        d->reqtype = 99;
        d->pa = pa;
        d->psi = pa->psi();
        d->jid = jid;
        d->pa->dialogRegister(this, d->jid);
        d->h = new EDBHandle(d->pa->edb());
        connect(d->h, SIGNAL(finished()), SLOT(edb_finished()));
        connect(ui_.searchField,SIGNAL(returnPressed()),SLOT(findMessages()));
        //connect(ui_.searchField,SIGNAL(textChanged(const QString)),SLOT(findMessages()));
        connect(ui_.buttonPrevious,SIGNAL(clicked()),SLOT(getPrevious()));
        connect(ui_.buttonNext,SIGNAL(clicked()),SLOT(getNext()));

        //connect(ui_.buttonRefresh,SIGNAL(released()),SLOT(getLatest()));
        connect(ui_.jidList, SIGNAL(itemSelectionChanged()), SLOT(openSelectedContact()));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/history").icon());
#endif

        listAccounts();
        ReadMessages();
}
void HistoryDlg::changeAccount(const QString accountName){
    d->pa = d->psi->contactList()->getAccountByJid(accountName);
    d->h = new EDBHandle(d->pa->edb()); //set handle to new EDB
    connect(d->h, SIGNAL(finished()), SLOT(edb_finished()));
    ReadMessages();
}

void HistoryDlg::listAccounts()
{
        if (d->psi) {
            foreach(PsiAccount* account, d->psi->contactList()->enabledAccounts())
                ui_.accountsBox->addItem(IconsetFactory::icon("psi/account").icon(),account->jid().full());
        }
        //select active account
        ui_.accountsBox->setCurrentIndex(ui_.accountsBox->findText(d->pa->jid().full()));
        //connect signal after the list is populated to prevent execution in the middle of the loop
        connect(ui_.accountsBox,SIGNAL(currentIndexChanged (const QString)),SLOT(changeAccount(const QString)));
}
void HistoryDlg::ReadMessages(){
    ui_.jidList->clear();
    ui_.msgLog->clear();
    foreach(PsiContact* contact,d->pa->contactList()){
        QListWidgetItem *item = new QListWidgetItem(contact->name(),ui_.jidList);
        item->setToolTip(contact->jid().full());
        item->setIcon(PsiIconset::instance()->status(contact->status()).icon());
        ui_.jidList->addItem(item);
    }
    //set contact in jidList to selected jid
    for(int i = 0; i < ui_.jidList->count(); i++){
        if (ui_.jidList->item(i)->toolTip() == d->jid.full())
            ui_.jidList->setCurrentRow(i); //triggers openSelectedContact()
    }

}
void HistoryDlg::openSelectedContact(){
    QListWidgetItem *item = ui_.jidList->currentItem();
    UserListItem *u = d->pa->findFirstRelevant(item->toolTip());
    if (u == NULL)
        return;
    setWindowTitle(u->name() + " (" + u->jid().full() + ")");
    d->jid = u->jid();
    getLatest();
}
void HistoryDlg::highlightBlocks(const QString text)
{
    if (text.isEmpty())
        return;
    QTextEdit::ExtraSelection highlight;
    highlight.format.setBackground(QColor(220,220,220)); // gray
    highlight.cursor = ui_.msgLog->textCursor();
    QList<QTextEdit::ExtraSelection> extras;
    QString plainText = ui_.msgLog->getPlainText();
    int index = plainText.indexOf(text,0,Qt::CaseInsensitive);
    int length = text.length();
    while (index >= 0){
        highlight.cursor.movePosition(QTextCursor::Start,QTextCursor::MoveAnchor); //jump to beginning
        highlight.cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::MoveAnchor,index); // and then jump "index" characters to the word that needs highlighting
        highlight.cursor.select(QTextCursor::WordUnderCursor); //TODO: does not highlight multiple words, needs improving
        extras << highlight;
        index = plainText.indexOf(text,index+length,Qt::CaseInsensitive);
    }
    ui_.msgLog->setExtraSelections( extras );
 }
void HistoryDlg::findMessages()
{
    d->reqtype = 3;
    d->h->getOldest(d->jid, 1);

}
void HistoryDlg::edb_finished()
{
        const EDBResult *r = d->h->result();
        if(d->h->lastRequestType() == EDBHandle::Read && r) {
            if(r->count() > 0) {
            if(d->reqtype == 0 || d->reqtype == 1) {
                    // events are in backward order
                    // first entry is the end event
                    EDBItem* it = r->first();
                    d->id_end = it->id();
                    d->id_next = it->nextId();
                    // last entry is the begin event
                    it = r->last();
                    d->id_begin = it->id();
                    d->id_prev = it->prevId();
                    displayResult(r, EDB::Forward);
            }
            else if(d->reqtype == 2) {
                    // events are in forward order
                    // last entry is the end event
                    EDBItem* it = r->last();
                    d->id_end = it->id();
                    d->id_next = it->nextId();
                    // first entry is the begin event
                    it = r->first();
                    d->id_begin = it->id();
                    d->id_prev = it->prevId();
                    displayResult(r, EDB::Forward);
            }
            else if(d->reqtype == 3) {
                QString str = ui_.searchField->text();
                if(str.isEmpty()){
                    getLatest();
                    return;
                }
                d->reqtype = 4;
                d->findStr = str;
                EDBItem *ei = r->first();
                d->h->find(str, d->jid, ei->id(), EDB::Forward);
            }
            else if(d->reqtype == 4) {
                    displayResult(r, EDB::Forward);
                    return;
            }
         }  else {
                //clear message log to remove previous results if no results found
                ui_.msgLog->clear();
                if(d->reqtype == 4) {
                    return;
                }
            }
        }
        setButtons();

}
void HistoryDlg::setButtons()
{
        ui_.buttonPrevious->setEnabled(!d->id_prev.isEmpty());
        ui_.buttonNext->setEnabled(!d->id_next.isEmpty());
}
void HistoryDlg::getLatest(){
    d->reqtype = 0;
    d->h->getLatest(d->jid, 50);

}

void HistoryDlg::getPrevious(){
    d->reqtype = 1;
    ui_.buttonPrevious->setEnabled(false);
    d->h->get(d->jid, d->id_prev, EDB::Backward, 50);

}
void HistoryDlg::getNext(){
    d->reqtype = 2;
    ui_.buttonNext->setEnabled(false);
    d->h->get(d->jid, d->id_next, EDB::Forward, 50);
}

void HistoryDlg::displayResult(const EDBResult *r, int direction, int max){
        int i = (direction == EDB::Forward) ? r->count()-1 : 0;
        int at = 0;
        ui_.msgLog->clear();
        while (i >= 0 && i <= r->count()-1 && (max == -1 ? true : at < max)) {
                EDBItem* item = r->value(i);
                PsiEvent* e = item->event();
                UserListItem *u = d->pa->findFirstRelevant(e->from().full());
                QString from = JIDUtil::nickOrJid(u->name(), u->jid().full());
                if(e->type() == PsiEvent::Message) {
                        MessageEvent *me = (MessageEvent *)e;
                        QString msg = me->message().body();
                        msg = TextUtil::linkify(TextUtil::plain2rich(msg));

                        if (PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool())
                            msg = TextUtil::emoticonify(msg);
                        if (PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
                            msg = TextUtil::legacyFormat(msg);

                        if (me->originLocal())
                            ui_.msgLog->appendText("<span style='color:red'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]")+" &lt;"+ d->pa->nick() +"&gt; " + msg + "</span>");
                        else
                            ui_.msgLog->appendText("<span style='color:blue'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]") + " &lt;" +  from + "&gt; " + msg + "</span>");
                }

                ++at;
                i += (direction == EDB::Forward) ? -1 : +1;
        }
        highlightBlocks(ui_.searchField->text());
        ui_.msgLog->verticalScrollBar()->setValue(0); //scroll to top
}
