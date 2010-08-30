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
        d->jid = jid;
        d->pa->dialogRegister(this, d->jid);
        d->h = new EDBHandle(d->pa->edb());
        connect(d->h, SIGNAL(finished()), SLOT(edb_finished()));
        connect(ui_.searchField,SIGNAL(returnPressed()),SLOT(findMessages()));
        connect(ui_.searchField,SIGNAL(textChanged()),SLOT(findMessages()));
        connect(ui_.buttonPrevious,SIGNAL(clicked()),SLOT(getPrevious()));
        connect(ui_.buttonNext,SIGNAL(clicked()),SLOT(getNext()));
        //connect(ui_.buttonRefresh,SIGNAL(released()),SLOT(getLatest()));
        connect(ui_.jidList, SIGNAL(itemSelectionChanged()), SLOT(openSelectedContact()));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/history").icon());
#endif
        ReadMessages();
}
void HistoryDlg::ReadMessages(){
    QDir historyDir(ApplicationInfo::historyDir());
    QFileInfoList list = historyDir.entryInfoList("*.history");
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        Jid contact = JIDUtil::fromString(JIDUtil::decode(fileInfo.completeBaseName()));

        UserListItem *u = d->pa->findFirstRelevant(contact.full());
        //skip contacts from different accounts (TODO: need better way to do this)
        if (u == NULL)
            continue;
        QString them = JIDUtil::nickOrJid(u->name(), u->jid().full());
        QListWidgetItem *item = new QListWidgetItem(them,ui_.jidList);
        item->setStatusTip(u->jid().full());
        ui_.jidList->addItem(item);
    }
    //set contact in jidList to selected jid
    for(int i = 0; i < ui_.jidList->count(); i++){
        if (ui_.jidList->item(i)->statusTip() == d->jid.full())
            ui_.jidList->setCurrentRow(i); //triggers openSelectedContact()
    }

}
void HistoryDlg::openSelectedContact(){
    QListWidgetItem *item = ui_.jidList->currentItem();
    UserListItem *u = d->pa->findFirstRelevant(item->statusTip());
    setWindowTitle(u->name() + " (" + u->jid().full() + ")");
    d->jid = u->jid();
    getLatest();
}
void HistoryDlg::findMessages()
{
        QString str = ui_.searchField->text();

        if(str.isEmpty()){
            getLatest();
            return;
        }


        //if(d->h->children().count() < 1)
        //        return;

        if(d->id_begin.isEmpty()) {
                QMessageBox::information(this, tr("Find"), tr("Already at beginning of message history."));
                return;
        }

        printf("searching for: [%s], starting at id=[%s]\n", str.latin1(), d->id_begin.latin1());
        d->reqtype = 3;
        d->findStr = str;
        d->h->find(str, d->jid, d->id_begin, EDB::Forward);
}
void HistoryDlg::doSomething(){

	ui_.msgLog->appendText("test msg!");
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
                    displayResult(r, EDB::Backward);
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
                    displayResult(r, EDB::Forward);
                    return;
            }
         }  else {
                if(d->reqtype == 3) {
                    QMessageBox::information(this, tr("Find"), tr("Search string '%1' not found.").arg(d->findStr));
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
    //QMessageBox::information(this,"edb-finished","getLatest");
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
                            ui_.msgLog->appendText("<span style='color:red'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]")+" &lt;Ty&gt; " + msg + "</span>");
                        else
                            ui_.msgLog->appendText("<span style='color:blue'>" + me->timeStamp().toString("[dd.MM.yyyy hh:mm:ss]") + " &lt;" +  from + "&gt; " + msg + "</span>");
                }

                ++at;
                i += (direction == EDB::Forward) ? -1 : +1;
        }
}
