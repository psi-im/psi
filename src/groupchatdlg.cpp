/*
 * groupchatdlg.cpp - dialogs for handling groupchat
 * Copyright (C) 2001-2002  Justin Karneges
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

// TODO: Move all the 'logic' of groupchats into MUCManager. See MUCManager
// for more details.

#include "groupchatdlg.h"

#include "accountlabel.h"
#include "avatars.h"
#include "avcall/avcall.h"
#include "bookmarkmanager.h"
#include "coloropt.h"
#include "filesharingmanager.h"
#include "gcuserview.h"
#include "groupchattopicdlg.h"
#include "iconaction.h"
#include "iconselect.h"
#include "iconwidget.h"
#include "infodlg.h"
#include "languagemanager.h"
#include "lastactivitytask.h"
#include "mcmdmanager.h"
#include "mcmdsimplesite.h"
#include "messageview.h"
#include "msgmle.h"
#include "mucconfigdlg.h"
#include "mucmanager.h"
#include "mucreasonseditor.h"
#include "pixmapratiolabel.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "iris/xmpp_caps.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_tasks.h"
#include "iris/xmpp_vcard4.h"
#include "popupmanager.h"
#include "psiaccount.h"
#include "psiactionlist.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psievent.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "psirichtext.h"
#include "psitooltip.h"
#include "shortcutmanager.h"
#include "statusdlg.h"
#include "tabcompletion.h"
#include "tabdlg.h"
#include "textutil.h"
#include "typeaheadfind.h"
#include "ui_mucinfo.h"
#include "urlobject.h"
#include "userlist.h"
#include "vcardfactory.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument> // for TextUtil::escape()
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define MCMDMUC "https://psi-im.org/ids/mcmd#mucmain"
#define MCMDMUCNICK "https://psi-im.org/ids/mcmd#mucnick"

static const QString geometryOption = "options.ui.muc.size";

//----------------------------------------------------------------------------
// StatusPingTask
//----------------------------------------------------------------------------
#include "filesharedlg.h"
#include "iris/xmpp_xmlcommon.h"

class StatusPingTask : public Task {
    Q_OBJECT
public:
    StatusPingTask(const Jid &myjid, Task *parent) : Task(parent), myjid_(myjid) { }

    void onGo()
    {
        iq_ = createIQ(doc(), "get", myjid_.full(), id());

        QDomElement ping = doc()->createElementNS("urn:xmpp:ping", "ping");
        iq_.appendChild(ping);
        timeout.setSingleShot(true);
        timeout.setInterval(1000 * 60 * 10);
        connect(&timeout, SIGNAL(timeout()), SLOT(timeout_triggered()));
        send(iq_);
    }

    bool take(const QDomElement &x)
    {
        if (!iqVerify(x, myjid_, id())) // , "urn:xmpp:ping"
            return false;

        if (x.attribute("type") == "result") {
            // something bad, we never reply to this stanza so someone
            // else got it.
            // FIXME seems to be no longer true
            // emit result(NotUs, id());
            emit result(LoggedIn, id());
            setSuccess();
        } else if (x.attribute("type") == "get") {
            // All went well!
            emit result(LoggedIn, id());
            setSuccess();
        } else {
            QDomElement tag = x.firstChildElement("error");
            if (tag.isNull()) {
                emit result(OtherErr, id());
            } else {
                XMPP::Stanza::Error err;
                err.fromXml(tag, client()->stream().baseNS());
                if (err.condition == XMPP::Stanza::Error::ErrorCond::ItemNotFound) {
                    emit result(NoSuch, id());
                } else if (err.condition == XMPP::Stanza::Error::ErrorCond::NotAcceptable) {
                    emit result(NotOccupant, id());
                } else {
                    emit result(OtherErr, id());
                }
                setSuccess();
            }
        }
        return true;
    }

    enum Result { NotOccupant, Timeout, NotUs, NoSuch, LoggedIn, OtherErr };

signals:
    void result(StatusPingTask::Result res, QString id);

private slots:
    void timeout_triggered()
    {
        emit result(Timeout, id());
        setSuccess();
    }

private:
    QDomElement iq_;
    Jid         myjid_;
    QString     xid;
    QTimer      timeout;
};

//----------------------------------------------------------------------------
// GCMainDlg
//----------------------------------------------------------------------------
class GCMainDlg::Private : public QObject, public MCmdProviderIface {
    Q_OBJECT
public:
    enum { Connecting, Connected, Idle, ForcedLeave };
    enum { TitleBM, TitleDisco, TitleVCard, TitleJid, TitleNone };
    Private(GCMainDlg *d) : mCmdManager(&mCmdSite), tabCompletion(this)
    {
        dlg           = d;
        nickSeparator = ":";
        nonAnonymous  = false;
        alert         = false;

        trackBar = false;
        mCmdManager.registerProvider(this);
        actions = new ActionList("", 0, false);
    }

    ~Private() { delete actions; }

    GCMainDlg                             *dlg;
    int                                    state;
    int                                    mucNameSource;
    MUCManager                            *mucManager;
    GCUserModel                           *usersModel;
    QString                                self, prev_self;
    QString                                mucName, discoMucName, discoMucDescription, vcardMucName;
    QString                                password;
    QMap<LanguageManager::LangId, QString> subjectMap;
    QString                                lastTopic;
    bool                                   nonAnonymous; // got status code 100 ?
    ActionList                            *actions;
    IconAction                            *act_bookmark, *act_pastesend;
    TypeAheadFindBar                      *typeahead;
    // #ifdef WHITEBOARDING
    //     IconAction *act_whiteboard;
    // #endif
    QAction *act_send, *act_scrollup, *act_scrolldown, *act_close;
    QAction *act_mini_cmd, *act_nick, *act_hide, *act_copy_muc_jid;
    QAction *act_minimize;

    MCmdSimpleSite mCmdSite;
    MCmdManager    mCmdManager;

    QString nickSeparator; // equals ":"

    QMenu *pm_settings;
    int    pending;
    int    hPending; // highlight pending
    bool   connecting;
    bool   alert;
    bool   gcSelfPresenceSupported = false;
    bool   gcSelfAvatarRequested   = false; // when self presence is not supported
    bool   forceQuit               = false; // ignore hide-when-closing option

    QStringList hist;
    int         histAt;

    QPointer<MUCConfigDlg>      configDlg;
    QPointer<GroupchatTopicDlg> topicDlg;

    int logSize;
    int rosterSize;

    int logHeight;
    int chateditHeight;

public:
    bool trackBar;
    bool tabmode;

public:
    ChatEdit *mle() const { return dlg->ui_.mle->chatEdit(); }
    ChatView *te_log() const { return dlg->ui_.log; }

public slots:
    void addEmoticon(const PsiIcon *icon) { addEmoticon(icon->defaultText()); }

    void addEmoticon(QString text)
    {
        if (!dlg->isActiveTab())
            return;

        PsiRichText::addEmoticon(mle(), text);
    }

    void sp_result(StatusPingTask::Result res, QString id)
    {
        // qDebug() << res;
        QString base = QString("Done Status ping (id=%1) ").arg(id);
        switch (res) {
        case StatusPingTask::NotOccupant:
            dlg->appendSysMsg(base + "NotOccupant", false);
            break;
        case StatusPingTask::Timeout:
            dlg->appendSysMsg(base + "Timeout", false);
            break;
        case StatusPingTask::NotUs:
            dlg->appendSysMsg(base + "NotUs", false);
            break;
        case StatusPingTask::NoSuch:
            dlg->appendSysMsg(base + "NoSuch", false);
            break;
        case StatusPingTask::LoggedIn:
            dlg->appendSysMsg(base + "LoggedIn", false);
            break;
        case StatusPingTask::OtherErr:
            dlg->appendSysMsg(base + "OtherErr", false);
            break;
        }
    }

    void version_finished()
    {
        JT_ClientVersion *version = static_cast<JT_ClientVersion *>(sender());
        if (!version->success()) {
            dlg->appendSysMsg(QString("No version information available for %1.").arg(version->jid().resource()),
                              false);
            return;
        }
        dlg->appendSysMsg(QString("Version response from %1: N: %2 V: %3 OS: %4")
                              .arg(version->jid().resource(), version->name(), version->version(), version->os()),
                          false);
    }

    void lastactivity_finished()
    {
        LastActivityTask *idle = static_cast<LastActivityTask *>(sender());

        if (!idle->success()) {
            dlg->appendSysMsg(QString("Can't determine last activity time for %1.").arg(idle->jid().resource()), false);
            return;
        }

        if (idle->status().isEmpty()) {
            dlg->appendSysMsg(
                QString("Last activity from %1 at %2").arg(idle->jid().resource(), idle->time().toString()), false);
        } else {
            dlg->appendSysMsg(QString("Last activity from %1 at %2 (%3)")
                                  .arg(idle->jid().resource(), idle->time().toString(), idle->status()),
                              false);
        }
    }

    void doSPing()
    {
        Jid             full = dlg->jid().withResource(self);
        StatusPingTask *sp   = new StatusPingTask(full, dlg->account()->client()->rootTask());
        connect(sp, SIGNAL(result(StatusPingTask::Result, QString)), SLOT(sp_result(StatusPingTask::Result, QString)));
        sp->go(true);
        dlg->appendSysMsg(QString("Doing Status ping (id=%1)").arg(sp->id()), false);
    }

    void doNick()
    {
        MCmdSimpleState *state = new MCmdSimpleState(MCMDMUCNICK, tr("new nick") + '=', MCMDSTATE_UNPARSED);
        connect(state, SIGNAL(unhandled(QStringList)), SLOT(NickComplete(QStringList)));
        mCmdManager.open(state, QStringList() << self);
    }

    bool NickComplete(QStringList command)
    {
        if (command.count() > 0) {
            QString nick = command[0].trimmed();
            if (!nick.isEmpty()) {
                prev_self = self;
                self      = nick;
                dlg->ui_.log->setLocalNickname(self);
                dlg->account()->groupChatChangeNick(dlg->jid().domain(), dlg->jid().node(), self,
                                                    dlg->account()->status());
            }
        }
        return true;
    }

    void doMiniCmd()
    {
        if (mCmdManager.isActive())
            mCmdManager.processCommand(QString());
        else
            mCmdManager.open(new MCmdSimpleState(MCMDMUC, tr("Command") + '>'), QStringList());
    }

public:
    virtual bool mCmdTryStateTransit(MCmdStateIface *oldstate, QStringList command, MCmdStateIface *&newstate,
                                     QStringList &preset)
    {
        if (oldstate->getName() == MCMDMUC) {
            QString cmd;
            if (command.count() > 0)
                cmd = command[0].toLower();
            /*
TODO:
part [message]

Maybe?:
join <channel>{,<channel>}
query <user>
join <channel>{,<channel>} [pass{,<pass>}
    */

            if (cmd == "clear") {
                dlg->doClear();
                histAt   = 0;
                newstate = nullptr;
            } else if (cmd == "nick") {
                if (command.count() > 1) {
                    QString nick = command[1].trimmed();
                    // FIXME nick can't be empty....
                    prev_self = self;
                    self      = nick;
                    dlg->ui_.log->setLocalNickname(self);
                    dlg->account()->groupChatChangeNick(dlg->jid().domain(), dlg->jid().node(), self,
                                                        dlg->account()->status());
                    newstate = nullptr;
                } else {
                    // FIXME DRY with doNick
                    MCmdSimpleState *state = new MCmdSimpleState("nick", tr("new nick") + '=', MCMDSTATE_UNPARSED);
                    connect(state, SIGNAL(unhandled(QStringList)), SLOT(NickComplete(QStringList)));
                    newstate = state;
                    preset   = QStringList() << self;
                }
            } else if (cmd == "sping") {
                doSPing();
                newstate = nullptr;
            } else if (cmd == "kick" && command.count() > 1) {
                command.removeFirst();
                const QString nick = command.takeFirst().trimmed();
                QString       reason;
                if (!command.isEmpty()) {
                    reason = command.join(" ");
                }
                mucManager->kick(nick, reason);
            } else if (cmd == "ban" && command.count() > 1) {
                command.removeFirst();
                QString                  nick    = command.takeFirst().trimmed();
                GCUserModel::MUCContact *contact = dlg->d->usersModel->findEntry(nick);
                if (contact && !contact->status.mucItem().jid().isEmpty()) {
                    nick = contact->status.mucItem().jid().bare();
                }
                QString reason;
                if (!command.isEmpty()) {
                    reason = command.join(" ").trimmed();
                }
                mucManager->ban(Jid(nick), reason);
            } else if (cmd == "invite" && command.count() > 1) {
                dlg->account()->actionInvite(Jid(command[1].trimmed()), dlg->jid().bare());
            } else if (cmd == "topic" && command.count() > 1) {
                command.removeFirst();
                const QString           topic = command.join(" ").trimmed();
                LanguageManager::LangId id; // no language identifier
                auto                    topicMap = dlg->d->subjectMap;
                topicMap.insert(id, topic);
                dlg->sendNewTopic(topicMap);
            } else if (cmd == "version" && command.count() > 1) {
                QString           nick    = command[1].trimmed();
                Jid               target  = dlg->jid().withResource(nick);
                JT_ClientVersion *version = new JT_ClientVersion(dlg->account()->client()->rootTask());
                connect(version, SIGNAL(finished()), SLOT(version_finished()));
                version->get(target);
                version->go();
                newstate = nullptr;
            } else if (cmd == "idle" && command.count() > 1) {
                QString           nick   = command[1].trimmed();
                Jid               target = dlg->jid().withResource(nick);
                LastActivityTask *idle   = new LastActivityTask(target, dlg->account()->client()->rootTask());
                connect(idle, SIGNAL(finished()), SLOT(lastactivity_finished()));
                idle->go();
                newstate = nullptr;
            } else if (cmd == "quote") {
                dlg->appendSysMsg(command.join("|"), false);
                preset   = command;
                newstate = oldstate;
                return true;
            } else if (cmd == "leave") {
                dlg->leave();
            } else if (!cmd.isEmpty()) {
                return false;
            }
        } else {
            return false;
        }

        return true;
    }

    virtual QStringList mCmdTryCompleteCommand(MCmdStateIface *state, QString query, QStringList partcommand, int item)
    {
        // qDebug() << "mCmdTryCompleteCommand " << item << ":" << query;
        QStringList all;
        if (state->getName() == MCMDMUC) {
            QString spaceAtEnd = QString(QChar(0));
            if (item == 0) {
                all << "clear" + spaceAtEnd << "nick" + spaceAtEnd << "kick" + spaceAtEnd << "leave" + spaceAtEnd
                    << "ban" + spaceAtEnd << "sping" + spaceAtEnd << "version" + spaceAtEnd << "invite" + spaceAtEnd
                    << "idle" + spaceAtEnd << "quote" + spaceAtEnd << "topic" + spaceAtEnd;
            } else if (item == 1) {
                if (partcommand[0] == "version" || partcommand[0] == "idle" || partcommand[0] == "kick"
                    || partcommand[0] == "ban") {
                    all = usersModel->nickList();
                } else if (partcommand[0] == "topic") {
                    LanguageManager::LangId id;
                    all << dlg->d->subjectMap.value(id);
                }
            } else if (item == 2) {
                if (partcommand[0] == "kick" || partcommand[0] == "ban") {
                    all << PsiOptions::instance()->getOption("options.muc.reasons").toStringList();
                }
            }
        }
        QStringList res;
        for (const QString &cmd : std::as_const(all)) {
            if (cmd.startsWith(query, Qt::CaseInsensitive)) {
                res << cmd;
            }
        }
        return res;
    }

    virtual void mCmdSiteDestroyed() { }

public:
    void doTrackBar()
    {
        trackBar = false;
        te_log()->doTrackBar();
    }
    void doFileShare(const QList<Reference> &refs, const QString &desc)
    {
        Message m(dlg->jid());
        m.setType(Message::Type::Groupchat);
        m.setReferences(refs);
        m.setBody(desc);
        emit dlg->aSend(m);
    }

public:
    QString lastReferrer; // contains nick of last person, who have said "yourNick: ..."

public slots:
    /** Insert a nick FIXME called from mini roster.
     */
    void insertNick(const QString &nick)
    {
        if (nick.isEmpty())
            return;

        QTextCursor cursor(mle()->textCursor());

        mle()->setUpdatesEnabled(false);
        cursor.beginEditBlock();

        int index = cursor.position();
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
        QString prev = cursor.selectedText();
        cursor.setPosition(index, QTextCursor::KeepAnchor);

        if (index > 0) {
            if (!prev.isEmpty() && !prev[0].isSpace())
                mle()->insertPlainText(" ");
            mle()->insertPlainText(nick);
        } else {
            mle()->insertPlainText(nick);
            mle()->insertPlainText(nickSeparator);
        }
        mle()->insertPlainText(" ");
        mle()->setFocus();

        cursor.endEditBlock();
        mle()->setUpdatesEnabled(true);
        mle()->viewport()->update();
    }

public:
    bool eventFilter(QObject *obj, QEvent *ev)
    {
        if (obj == te_log()->realTextWidget()) {
            if (ev->type() == QEvent::MouseButtonPress)
                mle()->setFocus();
            return QObject::eventFilter(obj, ev);
        }

        if (te_log()->handleCopyEvent(obj, ev, mle()))
            return true;

        if (obj == mle() && ev->type() == QEvent::KeyPress) {
            QKeyEvent *e = static_cast<QKeyEvent *>(ev);

            if (e->key() == Qt::Key_Tab) {
                tabCompletion.tryComplete();
                return true;
            }

            tabCompletion.reset();
            return false;
        }

        return QObject::eventFilter(obj, ev);
    }

    class TabCompletionMUC : public TabCompletion {
    public:
        GCMainDlg::Private *p_;
        TabCompletionMUC(GCMainDlg::Private *p) : p_(p), nickSeparator(":") {};

        virtual void setup(QString str, int pos, int &start, int &end)
        {
            if (p_->mCmdSite.isActive()) {
                mCmdList_ = p_->mCmdManager.completeCommand(str, pos, start, end);
            } else {
                TabCompletion::setup(str, pos, start, end);
            }
        }

        virtual QStringList possibleCompletions()
        {
            if (p_->mCmdSite.isActive()) {
                return mCmdList_;
            }
            QStringList suggestedNicks;
            QStringList nicks = allNicks();

            QString postAdd = atStart_ ? nickSeparator + " " : "";

            for (const QString &nick : nicks) {
                if (nick.left(toComplete_.length()).toLower() == toComplete_.toLower()) {
                    suggestedNicks << nick + postAdd;
                }
            }
            return suggestedNicks;
        };

        virtual QStringList allChoices(QString &guess)
        {
            if (p_->mCmdSite.isActive()) {
                guess = QString();
                return mCmdList_;
            }
            guess = p_->lastReferrer;
            if (!guess.isEmpty() && atStart_) {
                guess += nickSeparator + " ";
            }

            QStringList all = allNicks();

            if (atStart_) {
                QStringList::Iterator it = all.begin();
                for (; it != all.end(); ++it) {
                    *it = *it + nickSeparator + " ";
                }
            }
            return all;
        };

        QStringList allNicks() { return p_->dlg->d->usersModel->nickList(); }

        QStringList mCmdList_;

        // FIXME where to move this?
        QString nickSeparator; // equals ":"
    };

    TabCompletionMUC tabCompletion;
};

void GCMainDlg::openURL(const QString &url)
{
    if (url.startsWith("addnick://") && isActiveTab()) {
        const QString nick = QUrl::fromPercentEncoding(url.mid(14).toLatin1());
        onNickInsertClick(nick);
    }
}

void GCMainDlg::onNickInsertClick(const QString &nick)
{
    if (ui_.mle->chatEdit()->toPlainText().length() == 0) {
        ui_.mle->chatEdit()->insertPlainText(nick + QString(": "));
    } else {
        ui_.mle->chatEdit()->insertPlainText(QString(" %1 ").arg(nick));
    }
    ui_.mle->chatEdit()->setFocus();
}

void GCMainDlg::outgoingReactions(const QString &messageId, const QSet<QString> &reactions)
{
    Message m(jid());
    m.setType(Message::Type::Groupchat);
    m.setReactions({ messageId, reactions });
    QString id = account()->client()->genUniqueId();
    m.setId(id); // we need id early for message manipulations in chatview
    m.setTimeStamp(QDateTime::currentDateTime());
    // d->mle()->appendMessageHistory(m.body());

    emit aSend(m);
}

void GCMainDlg::sendMessageRetraction(const QString &messageId)
{
    Message m(jid());
    m.setType(Message::Type::Groupchat);
    m.setRetraction(messageId);
    // QString id = account()->client()->genUniqueId();
    // m.setId(id); // we need id early for message manipulations in chatview
    // m.setTimeStamp(QDateTime::currentDateTime());
    //  d->mle()->appendMessageHistory(m.body());

    emit aSend(m);
}

void GCMainDlg::doContactContextMenu(const QString &nick)
{
    auto itm = d->usersModel->findEntry(nick);
    if (!itm) {
        return;
    }

    if (!d->usersModel->selfContact()) {
        qWarning() << QString("groupchatdlg.cpp: Self ('%1') not found in contactlist").arg(d->self);
        return;
    }

    const MUCItem &smi = d->usersModel->selfContact()->status.mucItem();
    const MUCItem &lmi = itm->status.mucItem();

    bool     self = d->self == itm->name;
    QAction *act;
    QMenu   *pm = new QMenu();
    act         = new QAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &Message"), pm);
    pm->addAction(act);
    act->setData(0);
    act = new QAction(IconsetFactory::icon("psi/start-chat").icon(), tr("Open &Chat Window"), pm);
    pm->addAction(act);
    act->setData(1);

    act = new QAction(tr("E&xecute Command"), pm);
    pm->addAction(act);
    act->setData(6);

    pm->addSeparator();

    // Kick and Ban submenus
    QStringList reasons    = PsiOptions::instance()->getOption("options.muc.reasons").toStringList();
    int         cntReasons = reasons.count();
    if (cntReasons > 99)
        cntReasons = 99; // Only first 99 reasons

    QMenu *kickMenu = new QMenu(tr("&Kick"), pm);
    act             = new QAction(tr("No reason"), kickMenu);
    kickMenu->addAction(act);
    act->setData(10);
    act = new QAction(tr("Custom reason"), kickMenu);
    kickMenu->addAction(act);
    act->setData(100);
    kickMenu->addSeparator();
    bool canKick = MUCManager::canKick(smi, lmi);
    for (int i = 0; i < cntReasons; ++i) {
        act = new QAction(reasons[i], kickMenu);
        kickMenu->addAction(act);
        act->setData(101 + i);
    }
    kickMenu->setEnabled(canKick);

    QMenu *banMenu = new QMenu(tr("&Ban"), pm);
    act            = new QAction(tr("No reason"), banMenu);
    banMenu->addAction(act);
    act->setData(11);
    act = new QAction(tr("Custom reason"), banMenu);
    banMenu->addAction(act);
    act->setData(200);
    banMenu->addSeparator();
    bool canBan = MUCManager::canBan(smi, lmi);
    for (int i = 0; i < cntReasons; ++i) {
        act = new QAction(reasons[i], banMenu);
        banMenu->addAction(act);
        act->setData(201 + i);
    }
    banMenu->setEnabled(canBan);

    pm->addMenu(kickMenu);
    kickMenu->menuAction()->setEnabled(canKick);
    pm->addMenu(banMenu);
    banMenu->menuAction()->setEnabled(canBan);

    QMenu *rm = new QMenu(tr("Change Role"), pm);
    act       = new QAction(tr("Visitor"), rm);
    rm->addAction(act);
    act->setData(12);
    act->setCheckable(true);
    act->setChecked(lmi.role() == MUCItem::Visitor);
    act->setEnabled((!self || lmi.role() == MUCItem::Visitor) && MUCManager::canSetRole(smi, lmi, MUCItem::Visitor));

    act = new QAction(tr("Participant"), rm);
    rm->addAction(act);
    act->setData(13);
    act->setCheckable(true);
    act->setChecked(lmi.role() == MUCItem::Participant);
    act->setEnabled((!self || lmi.role() == MUCItem::Participant)
                    && MUCManager::canSetRole(smi, lmi, MUCItem::Participant));

    act = new QAction(tr("Moderator"), rm);
    rm->addAction(act);
    act->setData(14);
    act->setCheckable(true);
    act->setChecked(lmi.role() == MUCItem::Moderator);
    act->setEnabled((!self || lmi.role() == MUCItem::Moderator)
                    && MUCManager::canSetRole(smi, lmi, MUCItem::Moderator));
    pm->addMenu(rm);

    QMenu *am = new QMenu(tr("Change Affiliation"), pm);
    act       = am->addAction(tr("Unaffiliated"));
    act->setData(15);
    act->setCheckable(true);
    act->setChecked(lmi.affiliation() == MUCItem::NoAffiliation);
    act->setEnabled((!self || lmi.affiliation() == MUCItem::NoAffiliation)
                    && MUCManager::canSetAffiliation(smi, lmi, MUCItem::NoAffiliation));

    act = am->addAction(tr("Member"));
    act->setData(16);
    act->setCheckable(true);
    act->setChecked(lmi.affiliation() == MUCItem::Member);
    act->setEnabled((!self || lmi.affiliation() == MUCItem::Member)
                    && MUCManager::canSetAffiliation(smi, lmi, MUCItem::Member));

    act = am->addAction(tr("Administrator"));
    act->setData(17);
    act->setCheckable(true);
    act->setChecked(lmi.affiliation() == MUCItem::Admin);
    act->setEnabled((!self || lmi.affiliation() == MUCItem::Admin)
                    && MUCManager::canSetAffiliation(smi, lmi, MUCItem::Admin));

    act = am->addAction(tr("Owner"));
    act->setData(18);
    act->setCheckable(true);
    act->setChecked(lmi.affiliation() == MUCItem::Owner);
    act->setEnabled((!self || lmi.affiliation() == MUCItem::Owner)
                    && MUCManager::canSetAffiliation(smi, lmi, MUCItem::Owner));

    pm->addMenu(am);
    pm->addSeparator();
    // pm->insertItem(tr("Send &File"), 4);
    // pm->insertSeparator();
    // pm->insertItem(tr("Check &Status"), 2);

    act = new QAction(IconsetFactory::icon("psi/vCard").icon(), tr("User &Info"), pm);
    pm->addAction(act);
    act->setData(3);

    const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
    if (!css.isEmpty()) {
        pm->setStyleSheet(css);
    }
    int  x       = -1;
    bool enabled = false;
    act          = pm->exec(QCursor::pos());
    if (act) {
        x       = act->data().toInt();
        enabled = act->isEnabled();
    }
    delete pm;

    if (x == -1 || !enabled)
        return;
    lv_action(itm->name, itm->status, x);
}

GCMainDlg::GCMainDlg(PsiAccount *pa, const Jid &j, TabManager *tabManager) : TabbableWidget(j.bare(), pa, tabManager)
{
    setAttribute(Qt::WA_DeleteOnClose);
    d       = new Private(this);
    d->self = d->prev_self = j.resource();
    d->mucNameSource       = Private::TitleNone;
    account()->dialogRegister(this, jid());
    connect(account(), SIGNAL(updatedActivity()), SLOT(pa_updatedActivity()));
    d->mucManager = new MUCManager(account(), jid());

    d->pending    = 0;
    d->hPending   = 0;
    d->connecting = false;

    d->histAt = 0;
    // d->findDlg = 0;
    d->configDlg  = nullptr;
    d->usersModel = new GCUserModel(pa, j, this);

    d->state = Private::Connected;

    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        connect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
        connect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
        connect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
    }

    setAcceptDrops(true);

    ui_.setupUi(this);
    d->tabmode = PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool();

    ui_.log->setSessionData(true, false, jid(), jid().full()); // FIXME change conference name
    ui_.log->setLocalNickname(d->self);
#ifdef WEBKIT
    ui_.log->setAccount(account());
#else
    ui_.log->setMediaOpener(account()->fileSharingDeviceOpener());
#endif

    connect(ui_.log, SIGNAL(showNickMenu(QString)), this, SLOT(doContactContextMenu(QString)));
    connect(URLObject::getInstance(), SIGNAL(openURL(QString)), SLOT(openURL(QString)));
    connect(ui_.log, SIGNAL(nickInsertClick(QString)), SLOT(onNickInsertClick(QString)));
    connect(ui_.log, &ChatView::outgoingReactions, this, &GCMainDlg::outgoingReactions);
    connect(ui_.log, &ChatView::outgoingMessageRetraction, this, &GCMainDlg::sendMessageRetraction);

    PsiToolTip::install(ui_.le_topic);

    /*
    d->act_find = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this);
    connect(d->act_find, SIGNAL(triggered()), SLOT(openFind()));
    ui_.tb_find->setDefaultAction(d->act_find);
*/
    ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());

#ifdef Q_OS_MAC
    // seems its useless hack
    // connect(ui_.log, SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

    ui_.lv_users->setModel(d->usersModel);
    connect(ui_.lv_users, SIGNAL(contextMenuRequested(const QString &)), SLOT(doContactContextMenu(const QString &)));
    connect(ui_.lv_users, SIGNAL(action(const QString &, const Status &, int)),
            SLOT(lv_action(const QString &, const Status &, int)));
    connect(ui_.lv_users, SIGNAL(insertNick(const QString &)), d, SLOT(insertNick(const QString &)));
    for (int i = 0; i < GCUserModel::LastGroupRole; i++) {
        ui_.lv_users->setExpanded(d->usersModel->index(i, 0), true);
    }

    // typeahead find bar
    QHBoxLayout *hb3a = new QHBoxLayout();
    d->typeahead      = new TypeAheadFindBar(ui_.log->textWidget(), tr("Find toolbar"), nullptr);
    hb3a->addWidget(d->typeahead);
    ui_.vboxLayout1->addLayout(hb3a);

    ui_.lb_ident->setAccount(account());
    ui_.lb_ident->setShowJid(false);
    connect(account()->psi(), &PsiCon::accountCountChanged, this, &GCMainDlg::updateIdentityVisibility);

    ActionList *actList = account()->psi()->actionList()->actionLists(PsiActionList::Actions_Groupchat).at(0);
    for (const QString &name : actList->actions()) {
        auto action = actList->copyAction(name, this);
        d->actions->addAction(name, action);

        if (name == QString::fromLatin1("gchat_info")) {
            connect(action, &IconAction::triggered, this, &GCMainDlg::doShowInfo);
        } else if (name == QString::fromLatin1("gchat_clear")) {
            connect(action, SIGNAL(triggered()), SLOT(doClearButton()));
        } else if (name == QString::fromLatin1("gchat_find")) {
            // typeahead find
            connect(action, SIGNAL(triggered()), d->typeahead, SLOT(toggleVisibility()));
            // -- typeahead
        } else if (name == QString::fromLatin1("gchat_configure")) {
            connect(action, SIGNAL(triggered()), SLOT(configureRoom()));
        } else if (name == QString::fromLatin1("gchat_html_text")) {
            connect(action, &QAction::triggered, d->mle(), &ChatEdit::doHTMLTextMenu);
        } else if (name == QString::fromLatin1("gchat_icon")) {
            connect(account()->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), d, SLOT(addEmoticon(QString)));
            action->setMenu(pa->psi()->iconSelectPopup());
            ui_.tb_emoticons->setMenu(pa->psi()->iconSelectPopup());
        } else if (name == QString::fromLatin1("gchat_info")) {
            connect(action, SIGNAL(triggered()), SLOT(doInfo()));
        } else if (name == QString::fromLatin1("gchat_share_files")) {
            connect(action, &QAction::triggered, account(), [this]() {
                FileShareDlg::shareFiles(
                    account(), jid().withResource(d->self),
                    [this](const QList<XMPP::Reference> &&rl, const QString &desc) {
                        d->doFileShare(std::move(rl), desc);
                    },
                    this);
            });
        } else if (name == "gchat_pin_tab") {
            connect(action, SIGNAL(triggered()), SLOT(pinTab()));
        } else if (name == QLatin1String("gchat_templates")) {
            action->setMenu(getTemplateMenu());
        }
    }

    actList = account()->psi()->actionList()->actionLists(PsiActionList::Actions_Common).at(0);
    for (const QString &name : actList->actions()) {
        auto action = actList->copyAction(name, this);
        d->actions->addAction(name, action);
    }

    // #ifdef WHITEBOARDING
    //     d->act_whiteboard = new IconAction(tr("Open a Whiteboard"), "psi/whiteboard", tr("Open a &Whiteboard"), 0,
    //     this); connect(d->act_whiteboard, SIGNAL(triggered()), SLOT(openWhiteboard()));
    // #endif

    d->act_nick = new QAction(this);
    d->act_nick->setText(tr("Change Nickname..."));
    connect(d->act_nick, SIGNAL(triggered()), d, SLOT(doNick()));

    d->act_mini_cmd = new QAction(this);
    d->act_mini_cmd->setText(tr("Enter Command..."));
    connect(d->act_mini_cmd, SIGNAL(triggered()), d, SLOT(doMiniCmd()));
    addAction(d->act_mini_cmd);

    d->act_bookmark  = new IconAction(this);
    auto actSetTopic = d->actions->action("gchat_set_topic");
    connect(actSetTopic, &IconAction::triggered, this, &GCMainDlg::openTopic);
    connect(d->act_bookmark, SIGNAL(triggered()), SLOT(doBookmark()));
    ui_.le_topic->addAction(actSetTopic);
    ui_.le_topic->addAction(d->act_bookmark);

    d->act_copy_muc_jid = new QAction(tr("Copy Groupchat JID"), this);
    connect(d->act_copy_muc_jid, &QAction::triggered, this,
            [this]() { QApplication::clipboard()->setText(jid().bare()); });
    ui_.le_topic->addAction(d->act_copy_muc_jid);

    BookmarkManager *bm = account()->bookmarkManager();
    d->act_bookmark->setVisible(bm->isAvailable());
    if (bm->isAvailable()) {
        updateBookmarkIcon();
    }
    connect(bm, SIGNAL(availabilityChanged()), SLOT(updateBookmarkIcon()));
    connect(bm, SIGNAL(conferencesChanged(QList<ConferenceBookmark>)), SLOT(updateBookmarkIcon()));
    connect(bm, SIGNAL(conferencesChanged(QList<ConferenceBookmark>)), SLOT(updateMucName()));
    connect(bm, SIGNAL(bookmarksSaved()), SLOT(updateBookmarkIcon()));

    d->act_pastesend = new IconAction(tr("Paste and Send"), "psi/action_paste_and_send", tr("Paste and Send"), 0, this);
    connect(d->act_pastesend, SIGNAL(triggered()), SLOT(doPasteAndSend()));

    d->act_minimize = new QAction(this);
    connect(d->act_minimize, SIGNAL(triggered()), SLOT(doMinimize()));
    addAction(d->act_minimize);

    int s = PsiIconset::instance()->system().iconSize();
    ui_.toolbar->setIconSize(QSize(s, s));

    // #ifdef WHITEBOARDING
    //     ui_.toolbar->addAction(d->act_whiteboard);
    // #endif
    ui_.toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

    // Common actions
    d->act_send = new QAction(this);
    d->act_send->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(d->act_send);
    connect(d->act_send, SIGNAL(triggered()), SLOT(mle_returnPressed()));
    ui_.pb_send->setIcon(IconsetFactory::icon("psi/action_button_send").icon());
    connect(ui_.pb_send, SIGNAL(clicked()), SLOT(mle_returnPressed()));
    connect(ui_.pb_send, SIGNAL(customContextMenuRequested(const QPoint)), SLOT(sendButtonMenu()));
    d->act_close = new QAction(this);
    addAction(d->act_close);
    connect(d->act_close, &QAction::triggered, this, &GCMainDlg::leave);
    d->act_hide = new QAction(this);
    addAction(d->act_hide);
    connect(d->act_hide, SIGNAL(triggered()), SLOT(hideTab()));
    d->act_scrollup = new QAction(this);
    addAction(d->act_scrollup);
    connect(d->act_scrollup, SIGNAL(triggered()), SLOT(scrollUp()));
    d->act_scrolldown = new QAction(this);
    addAction(d->act_scrolldown);
    connect(d->act_scrolldown, SIGNAL(triggered()), SLOT(scrollDown()));

    ui_.mini_prompt->hide();
    connect(ui_.mle, &ChatEditProxy::textEditCreated, this, &GCMainDlg::chatEditCreated);
    chatEditCreated();
    ui_.log->init(); // we are ready to do that now. chatEditCreated() inited last pieces required for this init

    d->pm_settings = new QMenu(this);
    connect(d->pm_settings, SIGNAL(aboutToShow()), SLOT(buildMenu()));
    ui_.tb_actions->setMenu(d->pm_settings);
    ui_.tb_actions->setIcon(IconsetFactory::icon("psi/select").icon());

    connect(ui_.hsplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(horizSplitterMoved()));
    connect(ui_.vsplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(verticalSplitterMoved(int, int)));

    // resize the horizontal splitter
    d->logSize    = PsiOptions::instance()->getOption("options.ui.muc.log-width").toInt();
    d->rosterSize = PsiOptions::instance()->getOption("options.ui.muc.roster-width").toInt();
    QList<int> list;
    bool       leftRoster = PsiOptions::instance()->getOption("options.ui.muc.roster-at-left").toBool();
    if (leftRoster)
        list << d->rosterSize << d->logSize;
    else
        list << d->logSize << d->rosterSize;

    ui_.hsplitter->setSizes(list);

    if (leftRoster)
        ui_.hsplitter->insertWidget(0, ui_.lv_users); // Swap widgets

    // resize the vertical splitter
    d->logHeight      = PsiOptions::instance()->getOption("options.ui.chat.log-height").toInt();
    d->chateditHeight = PsiOptions::instance()->getOption("options.ui.chat.chatedit-height").toInt();
    QTimer::singleShot(0, this, [this]() { setVSplitterPosition(d->logHeight, d->chateditHeight); });

    X11WM_CLASS("groupchat");

    ui_.mle->chatEdit()->setFocus();
    ui_.log->realTextWidget()->installEventFilter(d);

    // Connect signals from MUC manager
    connect(d->mucManager, &MUCManager::action_error, this, &GCMainDlg::action_error);
    connect(d->mucManager, SIGNAL(action_success(MUCManager::Action)), d->usersModel, SLOT(updateAll()));

    updateMucName();
    updateGCVCard();
    updateConfiguration();

    setLooks();
    setToolbuttons();
    setShortcuts();
    invalidateTab();
    setConnecting();

    connect(ui_.log, &ChatView::quote, ui_.mle->chatEdit(), &ChatEdit::insertAsQuote);
    connect(ui_.log, &ChatView::editMessageRequested, ui_.mle->chatEdit(), &ChatEdit::startCorrection);
    connect(ui_.log, &ChatView::forwardMessageRequested, account(),
            [this](const QString &messageId, const QString &nick, const QString &text) {
                account()->psi()->invokeForwardMessage(messageId, jid().withResource(nick), text);
            });
    connect(ui_.log, &ChatView::openInfoRequested, account(),
            [this](const QString &nick) { account()->invokeGCInfo(jid().withResource(nick)); });
    connect(ui_.log, &ChatView::openChatRequested, account(),
            [this](const QString &nick) { account()->invokeGCChat(jid().withResource(nick)); });
    connect(pa->avatarFactory(), &AvatarFactory::avatarChanged, this, &GCMainDlg::avatarUpdated);

#ifdef PSI_PLUGINS
    PluginManager::instance()->setupGCTab(this, account(), jid().full());
#endif
}

GCMainDlg::~GCMainDlg()
{
    //  Save splitters size
    PsiOptions::instance()->setOption("options.ui.muc.log-width", d->logSize);
    PsiOptions::instance()->setOption("options.ui.muc.roster-width", d->rosterSize);

    if (d->state != Private::Idle && d->state != Private::ForcedLeave) {
        account()->groupChatLeave(jid().domain(), jid().node());
    }

    // QMimeSourceFactory *m = ui_.log->mimeSourceFactory();
    // ui_.log->setMimeSourceFactory(0);
    // delete m;

    account()->dialogUnregister(this);
    delete d->mucManager;
    delete d;

    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        disconnect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
        disconnect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
        disconnect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
    }
}

void GCMainDlg::horizSplitterMoved()
{
    QList<int> list       = ui_.hsplitter->sizes();
    bool       leftRoster = PsiOptions::instance()->getOption("options.ui.muc.roster-at-left").toBool();
    d->rosterSize         = !leftRoster ? list.last() : list.first();
    d->logSize            = leftRoster ? list.last() : list.first();

    PsiOptions::instance()->setOption("options.ui.muc.log-width", d->logSize);
    PsiOptions::instance()->setOption("options.ui.muc.roster-width", d->rosterSize);
}

void GCMainDlg::verticalSplitterMoved(int, int)
{
    QList<int> list   = ui_.vsplitter->sizes();
    d->logHeight      = list.first();
    d->chateditHeight = list.last();
    PsiOptions::instance()->setOption("options.ui.chat.log-height", d->logHeight);
    PsiOptions::instance()->setOption("options.ui.chat.chatedit-height", d->chateditHeight);

    emit vSplitterMoved(d->logHeight, d->chateditHeight);
}

void GCMainDlg::setVSplitterPosition(int log, int chat)
{
    QList<int> list;
    d->logHeight      = log;
    d->chateditHeight = chat;
    list << log << chat;
    ui_.vsplitter->setSizes(list);
}

void GCMainDlg::doMinimize() { window()->showMinimized(); }

void GCMainDlg::ensureTabbedCorrectly()
{
    TabbableWidget::ensureTabbedCorrectly();
    setShortcuts();
    // QSplitter is broken again, force resize so that
    // lv_users gets initizalised properly and context menu
    // works in tabs too.
    d->logSize    = PsiOptions::instance()->getOption("options.ui.muc.log-width").toInt();
    d->rosterSize = PsiOptions::instance()->getOption("options.ui.muc.roster-width").toInt();
    QList<int> list;
    if (PsiOptions::instance()->getOption("options.ui.muc.roster-at-left").toBool())
        list << d->rosterSize << d->logSize;
    else
        list << d->logSize << d->rosterSize;
    ui_.hsplitter->setSizes(QList<int>() << 0);
    ui_.hsplitter->setSizes(list);
    UserListItem *u = account()->find(jid().bare());
    if (u) {
        setTabIcon(PsiIconset::instance()->statusPtr(u)->icon());
    } else {
        setStatusTabIcon((d->state == Private::Connected ? STATUS_ONLINE : STATUS_OFFLINE));
    }
    if (!isTabbed() && geometryOptionPath().isEmpty()) {
        setGeometryOptionPath(geometryOption);
    }
}

void GCMainDlg::setShortcuts()
{

    d->actions->action("gchat_clear")->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
    d->actions->action("gchat_find")->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find"));
    d->act_send->setShortcuts(ShortcutManager::instance()->shortcuts("chat.send"));
    if (!isTabbed()) {
        d->act_close->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
    } else {
        d->act_close->QAction::setShortcuts(QList<QKeySequence>());
    }
    d->act_hide->setShortcuts(ShortcutManager::instance()->shortcuts("common.hide"));
    d->act_scrollup->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-up"));
    d->act_scrolldown->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-down"));
    d->act_mini_cmd->setShortcuts(ShortcutManager::instance()->shortcuts("chat.quick-command"));
    d->act_minimize->setShortcuts(ShortcutManager::instance()->shortcuts("chat.minimize"));
}

void GCMainDlg::scrollUp() { ui_.log->scrollUp(); }

void GCMainDlg::scrollDown() { ui_.log->scrollDown(); }

void GCMainDlg::closeEvent(QCloseEvent *e)
{
    if (!d->forceQuit && PsiOptions::instance()->getOption("options.ui.muc.hide-when-closing").toBool()
        && !isTabbed()) {
        hide();
        e->ignore();
        return;
    }
    e->accept();
    if (d->state != Private::Connected)
        account()->groupChatLeave(d->dlg->jid().domain(), d->dlg->jid().node());
}

void GCMainDlg::deactivated()
{
    TabbableWidget::deactivated();

    d->trackBar = true;
}

void GCMainDlg::activated()
{
    TabbableWidget::activated();

    if (d->pending > 0) {
        d->pending      = 0;
        d->hPending     = 0;
        UserListItem *u = account()->find(d->dlg->jid().bare());
        if (u) {
            u->setPending(d->pending, d->hPending);
            account()->updateEntry(*u);
        }
        emit messagesRead(jid());
        invalidateTab();
    }
    doFlash(false);

    ui_.mle->chatEdit()->setFocus();
    d->trackBar = false;
}

void GCMainDlg::mucInfoDialog(const QString &title, const QString &message, const MUCItem::Actor &actor,
                              const QString &reason)
{
    QString m = message;

    if (!actor.nick.isEmpty())
        m += tr(" by %1").arg(actor.nick);

    if (!actor.jid.isEmpty())
        m += tr(" by %1").arg(actor.jid.full());
    m += ".";

    if (!reason.isEmpty())
        m += tr("\nReason: %1").arg(reason);

    // FIXME maybe this should be queued in the future?
    QMessageBox *msg = new QMessageBox(QMessageBox::Information, title, m, QMessageBox::Ok, this);
    msg->setAttribute(Qt::WA_DeleteOnClose, true);
    msg->setModal(false);
    msg->show();
}

void GCMainDlg::logSelectionChanged()
{
#ifdef Q_OS_MAC
    // A hack to only give the message log focus when text is selected
// seems its already useless. at least copy works w/o this hack
//    if (ui_.log->textCursor().hasSelection())
//        ui_.log->setFocus();
//    else
//        ui_.mle->chatEdit()->setFocus();
#endif
}

void GCMainDlg::setConnecting()
{
    d->connecting = true;
    QTimer::singleShot(5000, this, SLOT(unsetConnecting()));
}

void GCMainDlg::updateBookmarkIcon()
{
    BookmarkManager *bm = account()->bookmarkManager();
    d->act_bookmark->setVisible(bm->isAvailable());
    if (bm->isAvailable()) {
        QString text;
        if (bm->isBookmarked(jid())) {
            text = tr("Edit Bookmark");
            d->act_bookmark->setPsiIcon("psi/bookmark_remove");
        } else {
            text = tr("Add to bookmarks");
            d->act_bookmark->setPsiIcon("psi/bookmark_add");
        }
        d->act_bookmark->setToolTip(text);
        d->act_bookmark->setText(text);
        d->act_bookmark->setStatusTip(text);
    }
}

#ifdef WHITEBOARDING
void GCMainDlg::openWhiteboard() { account()->actionOpenWhiteboardSpecific(jid(), jid().withResource(d->self), true); }
#endif

void GCMainDlg::unsetConnecting() { d->connecting = false; }

void GCMainDlg::action_error(MUCManager::Action, int, const QString &err) { appendSysMsg(err, false); }

void GCMainDlg::updateMucName()
{
    QString newName  = account()->bookmarkManager()->conferenceName(jid());
    d->mucNameSource = Private::TitleBM;
    if (newName.isEmpty()) {
        newName          = d->discoMucName;
        d->mucNameSource = Private::TitleDisco;
    }
    if (newName.isEmpty()) {
        newName          = d->vcardMucName;
        d->mucNameSource = Private::TitleVCard;
    }
    if (newName.isEmpty()) {
        newName          = jid().node();
        d->mucNameSource = Private::TitleJid;
    }
    if (newName != d->mucName) {
        d->mucName = newName;
        invalidateTab();
    }
}

void GCMainDlg::updateConfiguration()
{
    JT_DiscoInfo *disco = new JT_DiscoInfo(
        account()->client()->rootTask()); // FIXME in fact xep says we should do this before entering.
    connect(disco, SIGNAL(finished()), SLOT(discoInfoFinished())); // but we need this just for name for now.
    disco->get(jid());                                             // From other side we could provide the name outside.
    disco->go(true);
}

void GCMainDlg::discoInfoFinished()
{
    JT_DiscoInfo                *t = static_cast<JT_DiscoInfo *>(sender());
    const DiscoItem::Identities &i = t->item().identities();
    if (i.count() > 0) {
        d->discoMucName = i.first().name;
    }
    auto x = t->item().findExtension(XData::Data_Result, QLatin1String("http://jabber.org/protocol/muc#roominfo"));
    d->discoMucDescription = x.getField(QLatin1String("muc#roominfo_description")).value().value(0);
    if (d->mucNameSource >= Private::TitleDisco) {
        updateMucName();
    }
    if (!d->gcSelfPresenceSupported) {
        if (t->item().features().hasVCard()) {
            // xep-0486 says either we have "muc#roominfo_avatarhash" with valid hash
            // or don't have "muc#roominfo_avatarhash" at all and no avatar correspondingly.
            // unfortunatelly we also have old server without this stuff in disco.
            // So we are going to update avatar factory only we have "muc#roominfo_avatarhash".
            // A drawback of this is inability to reset avatar by room configuration update notification.
            auto field = x.findField(QLatin1String("muc#roominfo_avatarhash"));
            if (field) {
                auto avatarHash = field->value().value(0);
                account()->avatarFactory()->ensureVCardUpdated(jid(), QByteArray::fromHex(avatarHash.toLatin1()),
                                                               AvatarFactory::MucRoom);
            }
        }
    }
}

// this one is called as result of avatarChanged event from avatar factory or
void GCMainDlg::setMucSelfAvatar()
{
    bool    enabled = PsiOptions::instance()->getOption("options.ui.chat.avatars.show").toBool();
    QPixmap p;
    if (enabled) {
        p       = account()->avatarFactory()->getAvatar(jid().withResource(QString()));
        enabled = !p.isNull();
    }
    ui_.lblAvatar->setVisible(enabled);
    if (enabled) {
        int optSize = PsiOptions::instance()->getOption("options.ui.chat.avatars.size").toInt();
        ui_.lblAvatar->setResizePolicy(PixmapRatioLabel::Policy::FitVertical);
        ui_.lblAvatar->setMaxPixmapSize(QSize(optSize, optSize) * devicePixelRatio());
        ui_.lblAvatar->setPixmap(p);
    } else {
        ui_.lblAvatar->resize(0, 0);
    }
}

void GCMainDlg::updateGCVCard()
{
    const auto vcard = VCardFactory::instance()->vcard(jid());
    QPixmap    avatar;
    if (vcard) {
        d->vcardMucName = vcard.nickName().preferred().data.value(0);
        if (d->vcardMucName.isEmpty()) {
            d->vcardMucName = vcard.fullName().preferred();
        }
        if (d->mucNameSource >= Private::TitleVCard) {
            updateMucName();
        }
        avatar.loadFromData(vcard.photo());
    }
    // setMucSelfAvatar(avatar);
}

void MiniCommand_Depreciation_Message(const QString &old, const QString &newCmd, QString &line1, QString &line2)
{
    line1                    = QObject::tr("Warning: %1 is deprecated and will be removed in the future").arg(old);
    QList<QKeySequence> keys = ShortcutManager::instance()->shortcuts("chat.quick-command");
    if (keys.isEmpty()) {
        line2
            = QObject::tr("Please set a shortcut for 'Change to quick command mode', use that shortcut and enter '%1'.")
                  .arg(newCmd);
    } else {
        line2 = QObject::tr("Please instead press %1 and enter '%2'.").arg(keys.at(0).toString(), newCmd);
    }
}

void GCMainDlg::mle_returnPressed()
{
    d->tabCompletion.reset();
    QString str = d->mle()->toPlainText();

    if (d->mCmdSite.isActive()) {
        if (!d->mCmdManager.processCommand(str)) {
            appendSysMsg(tr("Error: Cannot parse command: ") + str, false);
        }
        return;
    }

    if (str.isEmpty())
        return;

    if (str == "/clear") {
        doClear();

        d->histAt = 0;
        d->hist.prepend(str);
        ui_.mle->chatEdit()->setText("");

        QString line1, line2;
        MiniCommand_Depreciation_Message("/clear", "clear", line1, line2);
        appendSysMsg(line1, false);
        appendSysMsg(line2, false);
        return;
    }

    if (str.startsWith("/nick ", Qt::CaseInsensitive)) {
        QString   nick   = str.mid(6).trimmed();
        XMPP::Jid newJid = jid().withResource(nick);
        if (!nick.isEmpty() && newJid.isValid()) {
            d->prev_self = d->self;
            d->self      = newJid.resource();
            ui_.log->setLocalNickname(d->self);
            account()->groupChatChangeNick(jid().domain(), jid().node(), d->self, account()->status());
        }
        ui_.mle->chatEdit()->setText("");
        QString line1, line2;
        MiniCommand_Depreciation_Message("/nick", "nick", line1, line2);
        appendSysMsg(line1, false);
        appendSysMsg(line2, false);
        return;
    }

    if (d->state != Private::Connected)
        return;

    Message m(jid());
    m.setType(Message::Type::Groupchat);
    m.setBody(str);
    QString id = account()->client()->genUniqueId();
    m.setId(id); // we need id early for message manipulations in chatview
    if (ui_.mle->chatEdit()->isCorrection()) {
        m.setReplaceId(ui_.mle->chatEdit()->correctionId());
    }
    ui_.mle->chatEdit()->setLastMessageId(id);
    ui_.mle->chatEdit()->resetCorrection();

    HTMLElement html = d->mle()->toHTMLElement();
    if (!html.body().isNull())
        m.setHTML(html);

    m.setTimeStamp(QDateTime::currentDateTime());
    d->mle()->appendMessageHistory(m.body());

    emit aSend(m);

    d->histAt = 0;
    d->hist.prepend(str);
    ui_.mle->chatEdit()->setText("");
}

/*void GCMainDlg::le_upPressed()
{
    if(d->histAt < (int)d->hist.count()) {
        ++d->histAt;
        d->le_input->setText(d->hist[d->histAt-1]);
    }
}

void GCMainDlg::le_downPressed()
{
    if(d->histAt > 0) {
        --d->histAt;
        if(d->histAt == 0)
            d->le_input->setText("");
        else
            d->le_input->setText(d->hist[d->histAt-1]);
    }
}*/

void GCMainDlg::openTopic()
{
    if (d->topicDlg) {
        ::bringToFront(d->topicDlg);
    } else {
        d->topicDlg = new GroupchatTopicDlg(this);
        d->topicDlg->setSubjectMap(d->subjectMap);
        d->topicDlg->setAttribute(Qt::WA_DeleteOnClose);
        d->topicDlg->show();
        QObject::connect(d->topicDlg, &GroupchatTopicDlg::accepted, this,
                         [this]() { sendNewTopic(d->topicDlg->subjectMap()); });
    }
}

void GCMainDlg::sendNewTopic(const QMap<LanguageManager::LangId, QString> &topics)
{
    Message m(jid());
    m.setType(Message::Type::Groupchat);
    for (auto it = topics.constBegin(); it != topics.constEnd(); ++it) {
        m.setSubject(it.value(), LanguageManager::toString(it.key()));
    }
    m.setTimeStamp(QDateTime::currentDateTime());
    emit aSend(m);
}

void GCMainDlg::doShowInfo()
{
    auto        dlg = new QDialog(this);
    Ui::MucInfo ui;
    ui.setupUi(dlg);
    dlg->setWindowTitle(getDisplayName());
    dlg->setWindowIcon(IconsetFactory::icon("psi/info").icon());
    ui.lblAccount->setAccount(account());
    ui.lblMucJid->setText(QString("<a href=\"xmpp:%1?join\">%1</a>").arg(jid().bare()));
    ui.lblDiscoName->setText(d->discoMucName);
    ui.lblMucDesc->setText(d->discoMucDescription);

    {
        QVBoxLayout *layout = new QVBoxLayout;
        const auto   vcard  = VCardFactory::instance()->vcard(jid());
        auto         info   = new InfoWidget(InfoWidget::MucView, jid(), vcard, account());
        layout->addWidget(info);
        ui.tab_vcard->setLayout(layout);
        // connect(vcard_, SIGNAL(busy()), ui_.busy, SLOT(start()));
        // connect(vcard_, SIGNAL(released()), ui_.busy, SLOT(stop()));
        info->doRefresh();
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void GCMainDlg::doClear() { ui_.log->clear(); }

void GCMainDlg::doClearButton()
{
    if (PsiOptions::instance()->getOption("options.ui.chat.warn-before-clear").toBool()) {
        auto ret = QMessageBox::warning(
            this, tr("Warning"),
            tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"),
            QMessageBox::Yes | QMessageBox::YesAll | QMessageBox::No);
        if (ret == QMessageBox::YesAll || ret == QMessageBox::Yes) {
            if (ret == QMessageBox::YesAll) {
                PsiOptions::instance()->setOption("options.ui.chat.warn-before-clear", false);
            }
            doClear();
        }
    } else {
        doClear();
    }
}

void GCMainDlg::doBookmark()
{
    BookmarkManager *bm = account()->bookmarkManager();
    if (!bm->isAvailable()) {
        return;
    }
    QList<ConferenceBookmark> confs   = bm->conferences();
    int                       confInd = bm->indexOfConference(jid());
    if (confInd < 0) { // not found
        ConferenceBookmark conf(getDisplayName(), jid(), ConferenceBookmark::Never, nick(), d->password);
        confs.push_back(conf);
        bm->setBookmarks(confs);
        return;
    }
    ConferenceBookmark &b = confs[confInd];

    QMenu *menu = new QMenu(this);

    menu->setAttribute(Qt::WA_DeleteOnClose);
    // menu->winId();
    // menu->windowHandle()->setTransientParent(window()->windowHandle());
    auto wa  = new QWidgetAction(menu);
    auto dlg = new QWidget(menu);
    wa->setDefaultWidget(dlg);

    QVBoxLayout *layout     = new QVBoxLayout(dlg);
    QHBoxLayout *blayout    = new QHBoxLayout;
    QFormLayout *formLayout = new QFormLayout;
    QLineEdit   *txtName    = new QLineEdit;
    QLineEdit   *txtNick    = new QLineEdit;
    // QCheckBox *chkAJoin = new QCheckBox;
    QComboBox *cbAutoJoin = new QComboBox;
    cbAutoJoin->addItems(ConferenceBookmark::joinTypeNames());
    QPushButton *saveBtn = new QPushButton(dlg->style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"), dlg);
    QPushButton *deleteBtn
        = new QPushButton(dlg->style()->standardIcon(QStyle::SP_DialogDiscardButton), tr("Delete"), dlg);

    blayout->insertStretch(0);
    blayout->addWidget(saveBtn);
    blayout->addWidget(deleteBtn);
    txtName->setText(b.name());
    txtNick->setText(b.nick());
    cbAutoJoin->setCurrentIndex(b.autoJoin());
    cbAutoJoin->setEditable(false);
    formLayout->addRow(tr("&Name:"), txtName);
    formLayout->addRow(tr("N&ick:"), txtNick);
    formLayout->addRow(tr("&Auto join:"), cbAutoJoin);
    layout->addLayout(formLayout);
    layout->addLayout(blayout);
    dlg->setMinimumWidth(300);
    dlg->connect(saveBtn, &QPushButton::clicked, this, [menu, txtName, txtNick, cbAutoJoin, this](bool) {
        ConferenceBookmark conf(txtName->text(), jid(), ConferenceBookmark::JoinType(cbAutoJoin->currentIndex()),
                                txtNick->text(), d->password);

        auto bm = account()->bookmarkManager();
        if (bm->isAvailable()) {
            int                       confInd = bm->indexOfConference(jid());
            QList<ConferenceBookmark> confs   = bm->conferences();

            if (confInd == -1) {
                confs.append(conf);
            } else {
                confs[confInd] = conf;
            }
            bm->setBookmarks(confs);

            if (getDisplayName() != txtName->text())
                account()->actionRename(jid(), txtName->text());
        }
        menu->close();
    });
    dlg->connect(deleteBtn, &QPushButton::clicked, this, [menu, this]() {
        BookmarkManager *bm = account()->bookmarkManager();
        if (bm->isAvailable()) {
            bm->removeConference(jid());
        }
        menu->close();
    });

    menu->addAction(wa);
    menu->popup(ui_.le_topic->mapToGlobal(QPoint(ui_.le_topic->width() - dlg->width(), ui_.le_topic->height())));
}

void GCMainDlg::configureRoom()
{
    if (d->configDlg)
        ::bringToFront(d->configDlg);
    else {
        auto                 c           = d->usersModel->selfContact();
        MUCItem::Role        role        = c ? c->status.mucItem().role() : MUCItem::UnknownRole;
        MUCItem::Affiliation affiliation = c ? c->status.mucItem().affiliation() : MUCItem::UnknownAffiliation;
        d->configDlg                     = new MUCConfigDlg(d->mucManager, this);
        d->configDlg->setRoleAffiliation(role, affiliation);
        d->configDlg->show();
    }
}

void GCMainDlg::goDisc()
{
    if (d->state != Private::Idle && d->state != Private::ForcedLeave) {
        d->state = Private::Idle;
        d->actions->action("gchat_set_topic")->setEnabled(false);
        setStatusTabIcon(STATUS_OFFLINE);
        appendSysMsg(tr("Disconnected."), true);
        ui_.mle->chatEdit()->setEnabled(false);
    }
}

// kick, ban, removed muc, etc
void GCMainDlg::goForcedLeave()
{
    if (d->state != Private::Idle && d->state != Private::ForcedLeave) {
        goDisc();
        account()->groupChatLeave(jid().domain(), jid().node());
        d->state = Private::ForcedLeave;
    }
}

bool GCMainDlg::isInactive() const { return d->state == Private::ForcedLeave; }

void GCMainDlg::reactivate()
{
    d->state = Private::Idle;
    goConn();
}

void GCMainDlg::setJid(const Jid &j)
{
    TabbableWidget::setJid(j);
    d->self = d->prev_self = j.resource();
    ui_.log->setLocalNickname(d->self);
}

void GCMainDlg::goConn()
{
    if (d->state == Private::Idle) {
        d->state = Private::Connecting;
        appendSysMsg(tr("Reconnecting..."), true);

        QString host = jid().domain();
        QString room = jid().node();
        QString nick = d->self;

        if (!account()->groupChatJoin(host, room, nick, d->password)) {
            appendSysMsg(tr("Error: You are in or joining this room already!"), true);
            d->state = Private::Idle;
        }
    }
}

void GCMainDlg::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasText())
        e->accept();
}

void GCMainDlg::dropEvent(QDropEvent *e)
{
    Jid droppedJid(e->mimeData()->text());
    if (droppedJid.isValid() && !droppedJid.node().isEmpty() && !d->usersModel->hasJid(droppedJid)) {
        Message m;
        m.setTo(this->jid());
        m.addMUCInvite(MUCInvite(droppedJid));
        if (!d->password.isEmpty())
            m.setMUCPassword(d->password);
        m.setTimeStamp(QDateTime::currentDateTime());
        account()->dj_sendMessage(m);
    } else {
        FileShareDlg::shareFiles(
            account(), jid().withResource(d->self), e->mimeData(),
            [this](const QList<XMPP::Reference> &&rl, const QString &desc) { d->doFileShare(std::move(rl), desc); },
            this);
    }
}

void GCMainDlg::pa_updatedActivity()
{
    if (!account()->loggedIn()) {
        goDisc();
    } else {
        if (d->state == Private::Idle) {
            goConn();
        } else if (d->state == Private::Connected) {
            Status s = account()->status();
            s.setXSigned("");
            account()->groupChatSetStatus(jid().domain(), jid().node(), s);
        }
    }
}

PsiAccount *GCMainDlg::account() const { return TabbableWidget::account(); }

void GCMainDlg::leave()
{
    d->forceQuit = true;
    close();
}

void GCMainDlg::error(int, const QString &str)
{
    d->actions->action("gchat_set_topic")->setEnabled(false);
    setStatusTabIcon(STATUS_ERROR);

    if (d->state == Private::Connecting)
        appendSysMsg(tr("Unable to join groupchat.    Reason: %1").arg(str), true);
    else
        appendSysMsg(tr("Unexpected groupchat error: %1").arg(str), true);

    d->state = Private::Idle;
}

void GCMainDlg::mucKickMsgHelper(const QString &nick, const Status &s, const QString &nickJid, const QString &title,
                                 const QString &youSimple, const QString &youBy, const QString &someoneSimple,
                                 const QString &someoneBy)
{
    QString message;

    QString     actor;
    const auto &actorRef = s.mucItem().actor();
    if (!actorRef.nick.isEmpty()) {
        actor = actorRef.nick;
    } else if (!actorRef.jid.isEmpty()) {
        actor = actorRef.jid.full();
    }
    if (nick == d->self) {
        message = youSimple;
        mucInfoDialog(title, message, s.mucItem().actor(), s.mucItem().reason());
        if (!actor.isEmpty()) {
            message = youBy.arg(actor);
        }
        goForcedLeave();
    } else if (!actor.isEmpty()) {
        message = someoneBy.arg(nickJid, actor);
    } else {
        message = someoneSimple.arg(nickJid);
    }

    if (!s.mucItem().reason().isEmpty()) {
        message += QString(" (%1)").arg(s.mucItem().reason());
    }
    appendSysMsg(message);
}

// presence from muc itself (resource is empty)
void GCMainDlg::gcSelfPresence(const Status &s)
{
    d->gcSelfPresenceSupported = true;
    account()->avatarFactory()->statusUpdate(jid(), s, AvatarFactory::MucRoom);
}

void GCMainDlg::presence(const QString &nick, const Status &s)
{
    if (s.hasError()) {
        QString message;
        if (s.errorCode() == 409) {
            message = tr("Please choose a different nickname");
            d->self = d->prev_self;
            ui_.log->setLocalNickname(d->self);
        } else {
            message = tr("An error occurred (errorcode: %1)").arg(s.errorCode());
        }
        appendSysMsg(message);
        return;
    }

    if ((nick.isEmpty()) && (s.getMUCStatuses().contains(100))) {
        d->nonAnonymous = true;
    }

    bool isSelf = (nick == d->self);
    if (isSelf) {
        if (s.isAvailable())
            setStatusTabIcon(s.type());
        UserListItem *u = account()->find(d->dlg->jid().bare());
        if (u) {
            Resource r;
            r.setName("Muc");
            r.setStatus(s);
            u->userResourceList().replace(0, r);
            account()->updateEntry(*u);
        }
        // Update configuration dialog
        if (d->configDlg) {
            d->configDlg->setRoleAffiliation(s.mucItem().role(), s.mucItem().affiliation());
        }
        d->actions->action("gchat_configure")->setEnabled(s.mucItem().affiliation() >= MUCItem::Member);
    }

    PsiOptions *options_ = PsiOptions::instance();

    if (s.isAvailable()) {
        // Available
        if (s.getMUCStatuses().contains(201)) {
            appendSysMsg(tr("New room created"));
            if (options_->getOption("options.muc.accept-defaults").toBool()) {
                d->mucManager->setDefaultConfiguration();
            } else if (options_->getOption("options.muc.auto-configure").toBool()) {
                QTimer::singleShot(0, this, SLOT(configureRoom()));
            }
        }

        auto contact = d->usersModel->findEntry(nick);
        if (contact == nullptr) {
            // contact joining
            // ui_.log->updateAvatar(jid().withResource(nick), isSelf? ChatViewCommon::LocalParty:
            // ChatViewCommon::Participant);

            MessageView mv(MessageView::MUCJoin);
            if ((!d->connecting || options_->getOption("options.ui.muc.show-initial-joins").toBool())
                && options_->getOption("options.muc.show-joins").toBool()) {
                QString message = tr("%1 has joined the room");
                if (options_->getOption("options.muc.show-role-affiliation").toBool()) {
                    if (s.mucItem().role() != MUCItem::NoRole) {
                        if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
                            message = tr("%3 has joined the room as %1 and %2")
                                          .arg(MUCManager::roleToString(s.mucItem().role(), true),
                                               MUCManager::affiliationToString(s.mucItem().affiliation(), true));
                        } else {
                            message = tr("%2 has joined the room as %1")
                                          .arg(MUCManager::roleToString(s.mucItem().role(), true));
                        }
                    } else if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
                        message = tr("%2 has joined the room as %1")
                                      .arg(MUCManager::affiliationToString(s.mucItem().affiliation(), true));
                    }
                }
                if (!s.mucItem().jid().isEmpty()) {
                    message = message.arg(QString("%1 (%2)").arg(nick, s.mucItem().jid().full()));
                } else {
                    message = message.arg(nick);
                }

                bool showStatusChanges = options_->getOption("options.muc.show-status-changes").toBool();
                if (showStatusChanges) {
                    message += tr(" and now is %1").arg(status2txt(s.type()));
                }

                mv = MessageView::mucJoinMessage(nick, int(s.type()), message, s.status(), s.priority());
                mv.setStatusChangeHidden(!showStatusChanges);
            } else {
                mv = MessageView::mucJoinMessage(nick, int(s.type()), QString(), s.status(), s.priority());
                mv.setStatusChangeHidden();
                mv.setJoinLeaveHidden();
            }
            mv.setLocal(isSelf); // hack
            dispatchMessage(mv);
        } else {
            // Status change
            if (!d->connecting && options_->getOption("options.muc.show-role-affiliation").toBool()) {
                QString message;
                QString reason;
                if (contact->status.mucItem().role() != s.mucItem().role() && s.mucItem().role() != MUCItem::NoRole) {
                    if (contact->status.mucItem().affiliation() != s.mucItem().affiliation()) {
                        message = tr("%1 is now %2 and %3")
                                      .arg(nick, MUCManager::roleToString(s.mucItem().role(), true),
                                           MUCManager::affiliationToString(s.mucItem().affiliation(), true));
                    } else {
                        message = tr("%1 is now %2").arg(nick, MUCManager::roleToString(s.mucItem().role(), true));
                    }
                    reason = s.mucItem().reason();
                } else if (contact->status.mucItem().affiliation() != s.mucItem().affiliation()) {
                    message += tr("%1 is now %2")
                                   .arg(nick, MUCManager::affiliationToString(s.mucItem().affiliation(), true));
                    reason = s.mucItem().reason();
                }

                if (!reason.isEmpty()) {
                    message += tr(" (Reason: %1)").arg(reason);
                }

                if (!message.isEmpty()) {
                    appendSysMsg(message);
                }
            }
            if (!d->connecting && options_->getOption("options.muc.show-status-changes").toBool()) {
                bool statusWithPriority = options_->getOption("options.ui.muc.status-with-priority").toBool();
                if (s.status() != contact->status.status() || s.show() != contact->status.show()
                    || (statusWithPriority && s.priority() != contact->status.priority())) {
                    ui_.log->dispatchMessage(MessageView::statusMessage(nick, int(s.type()), s.status(), s.priority()));
                }
            }
        }
        d->usersModel->updateEntry(nick, s);
        // if(!nick.isEmpty())
        //    avatarUpdated(jidForNick(nick)); // only by event from AvatarFactory we should do this
    } else {
        // Unavailable
        if (s.hasMUCDestroy()) {
            // Room was destroyed
            QString message = tr("This room has been destroyed.");
            QString log     = message;
            if (!s.mucDestroy().reason().isEmpty()) {
                message += "\n";
                QString reason = tr("Reason: %1").arg(s.mucDestroy().reason());
                message += reason;
                log += " " + reason;
            }
            if (!s.mucDestroy().jid().isEmpty()) {
                message += "\n";
                message += tr("Do you want to join the alternate venue '%1'?").arg(s.mucDestroy().jid().full());
                int ret
                    = QMessageBox::information(this, tr("Room Destroyed"), message, QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) {
                    account()->actionJoin(s.mucDestroy().jid().full());
                }
            } else {
                QMessageBox::information(this, tr("Room Destroyed"), message);
            }
            appendSysMsg(log);
            goForcedLeave();
        }

        QString message;
        QString nickJid;
        auto    contact = d->usersModel->findEntry(nick);
        if (contact && !contact->status.mucItem().jid().isEmpty()) {
            nickJid = QString("%1 (%2)").arg(nick, contact->status.mucItem().jid().full());
        } else {
            nickJid = nick;
        }

        bool suppressDefault = false;

        if (s.getMUCStatuses().contains(301)) {
            // Ban
            mucKickMsgHelper(nick, s, nickJid, tr("Banned"), tr("You have been banned from the room"),
                             tr("You have been banned from the room by %1"), tr("%1 has been banned"),
                             tr("%1 has been banned by %2"));
            suppressDefault = true;
        }
        if (s.getMUCStatuses().contains(333)) {
            if (nick == d->self || options_->getOption("options.ui.muc.show-technical-kicks").toBool())
                mucKickMsgHelper(nick, s, nickJid, tr("Removed"),
                                 tr("You have been removed from the room due to technical problem"),
                                 tr("You have been removed from the room by %1 due to technical problem"),
                                 tr("%1 has been removed from the room due to technical problem"),
                                 tr("%1 has been removed from the room by %2 due to technical problem"));
            suppressDefault = true;
        } else // 333 and 307 can come together. so "else" is here
            if (s.getMUCStatuses().contains(307)) {
                // Kick
                mucKickMsgHelper(nick, s, nickJid, tr("Kicked"), tr("You have been kicked from the room"),
                                 tr("You have been kicked from the room by %1"), tr("%1 has been kicked"),
                                 tr("%1 has been kicked by %2"));
                suppressDefault = true;
            }
        if (s.getMUCStatuses().contains(321)) {
            // Remove due to affiliation change
            mucKickMsgHelper(nick, s, nickJid, tr("Removed"),
                             tr("You have been removed from the room due to an affiliation change"),
                             tr("You have been removed from the room by %1 due to an affiliation change"),
                             tr("%1 has been removed from the room due to an affilliation change"),
                             tr("%1 has been removed from the room by %2 due to an affilliation change"));
            suppressDefault = true;
        }
        if (s.getMUCStatuses().contains(322)) {
            mucKickMsgHelper(nick, s, nickJid, tr("Removed"),
                             tr("You have been removed from the room because the room was made members only"),
                             tr("You have been removed from the room by %1 because the room was made members only"),
                             tr("%1 has been removed from the room because the room was made members-only"),
                             tr("%1 has been removed from the room by %2 because the room was made members-only"));
            suppressDefault = true;
        }

        if (!d->connecting && !suppressDefault && options_->getOption("options.muc.show-joins").toBool()) {
            if (s.getMUCStatuses().contains(303)) {
                message = tr("%1 is now known as %2").arg(nick, s.mucItem().nick());
                d->usersModel->updateEntry(s.mucItem().nick(), s);
                dispatchMessage(MessageView::nickChangeMessage(nick, s.mucItem().nick()));
            } else {
                // contact leaving
                message = tr("%1 has left the room").arg(nickJid);
                if (!s.status().isEmpty()) {
                    message += QString(" (%1)").arg(s.status());
                }
                dispatchMessage(MessageView::mucPartMessage(nick, message, s.status()));
            }
        } else {
            MessageView mv = MessageView::mucPartMessage(nick);
            mv.setJoinLeaveHidden();
            dispatchMessage(mv);
        }
        d->usersModel->removeEntry(nick);
    }

    if (s.caps().isValid()) {
        Jid caps_jid(s.mucItem().jid().isEmpty() || !d->nonAnonymous ? Jid(jid()).withResource(nick)
                                                                     : s.mucItem().jid());
        account()->client()->capsManager()->updateCaps(caps_jid, s.caps());
    }

    if (!nick.isEmpty())
        account()->avatarFactory()->statusUpdate(jidForNick(nick), s, AvatarFactory::MucUser);
}

XMPP::Jid GCMainDlg::jidForNick(const QString &nick) const { return Jid(jid()).withResource(nick); }

void GCMainDlg::avatarUpdated(const Jid &jid_)
{
    if (jid_.compare(jid(), false)) {
        if (jid_.resource().isEmpty()) {
            ui_.log->updateAvatar(jid_, ChatViewCommon::RemoteParty);
            setMucSelfAvatar();
            return;
        }
        d->usersModel->updateAvatar(jid_.resource());
        ui_.log->updateAvatar(jid_,
                              jid_.resource() == d->self ? ChatViewCommon::LocalParty : ChatViewCommon::Participant);
    }
}

void GCMainDlg::message(const Message &_m, const PsiEvent::Ptr &e)
{
    Message       m    = _m;
    const Message dm   = m.displayMessage();
    QString       from = dm.from().resource();
    d->alert           = false;

    if (dm.getMUCStatuses().contains(100)) {
        d->nonAnonymous = true;
    }
    if (dm.getMUCStatuses().contains(172)) {
        d->nonAnonymous = true;
    }
    if (dm.getMUCStatuses().contains(173)) {
        d->nonAnonymous = false;
    }
    if (dm.getMUCStatuses().contains(174)) {
        d->nonAnonymous = false;
    }
    if (dm.getMUCStatuses().contains(104)) {
        updateConfiguration();
    }

    PsiOptions *options = PsiOptions::instance();

    QString topic;
    if (!dm.subjectMap().isEmpty() && dm.isPureSubject()) {
        d->subjectMap.clear();
        auto sm = dm.subjectMap();
        for (auto l = sm.constBegin(); l != sm.constEnd(); ++l) {
            d->subjectMap.insert(LanguageManager::fromString(l.key()), l.value());
        }
        auto langs = d->subjectMap.keys();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        auto langsSet         = QSet<LanguageManager::LangId>(langs.begin(), langs.end());
        auto preferredSubject = LanguageManager::bestUiMatch(langsSet, true);
#else
        auto preferredSubject = LanguageManager::bestUiMatch(langs.toSet(), true);
#endif
        if (preferredSubject.count()) {
            topic = d->subjectMap.value(preferredSubject.first());
        }
    }

    if (!topic.isNull()) {
        if (d->lastTopic == topic) {
            return; // ignore same topic
        }
        d->lastTopic           = topic;
        QString subjectTooltip = TextUtil::plain2rich(topic);
        subjectTooltip         = TextUtil::linkify(subjectTooltip);
        if (options->getOption("options.ui.emoticons.use-emoticons").toBool()) {
            subjectTooltip = TextUtil::emoticonify(subjectTooltip);
        }

        QString sysMsg;
        if (from.isEmpty()) {
            // The topic was set by the server
            // ugly trick
            int btStart = dm.body().indexOf(topic);
            sysMsg      = btStart > 0 ? dm.body().left(btStart).remove(": ") : tr("The topic has been set to");
        } else {
            sysMsg = QString(from) + (topic.isEmpty() ? tr(" has unset the topic") : tr(" has set the topic to"));
        }
        MessageView tv = MessageView::subjectMessage(topic, sysMsg);
        tv.setDateTime(dm.timeStamp());

        ui_.le_topic->setText(topic.replace("\n\n", " || ")
                                  .replace("\n", " | ")
                                  .replace("\t", " ")
                                  .replace(QRegularExpression("\\s{2,}"), " "));
        ui_.le_topic->setCursorPosition(0);
        ui_.le_topic->setToolTip(QString("<qt><p>%1</p></qt>").arg(subjectTooltip));

        dispatchMessage(tv);
        return;
    }

    if (!dm.reactions().targetId.isEmpty()) {
        auto mv = MessageView::reactionsMessage(from, dm.reactions().targetId, dm.reactions().reactions);
        ui_.log->dispatchMessage(mv);
        return;
    }

    if (!dm.retraction().isEmpty()) {
        auto mv = MessageView::retractionMessage(dm.retraction());
        ui_.log->dispatchMessage(mv);
        return;
    }

    if (dm.body().isEmpty())
        return;

    // code to determine if the speaker was addressing this client in chat
    if (dm.body().contains(d->self))
        d->alert = true;

    if (dm.body().left(d->self.length()) == d->self)
        d->lastReferrer = dm.from().resource();

    if (options->getOption("options.ui.muc.use-highlighting").toBool()) {
        QStringList highlightWords = options->getOption("options.ui.muc.highlight-words").toStringList();
        for (const QString &word : highlightWords) {
            if (dm.body().contains((word), Qt::CaseInsensitive)) {
                d->alert = true;
            }
        }
    }

    // play sound?
    if (from == d->self) {
        if (!dm.spooled())
            account()->playSound(PsiAccount::eSend);
    } else {
        if (d->alert
            || (options->getOption("options.ui.notifications.sounds.notify-every-muc-message").toBool() && !dm.spooled()
                && !from.isEmpty()))
            account()->playSound(PsiAccount::eGroupChat);

        if (d->alert
            || (options->getOption("options.ui.notifications.passive-popups.notify-every-muc-message").toBool()
                && !dm.spooled() && !from.isEmpty())) {
            if (!dm.spooled() && !isActiveTab() && !dm.from().resource().isEmpty()) {
                XMPP::Jid    jid = dm.from() /*.withDomain("")*/;
                UserListItem i;
                i.setPrivate(true);
                account()->psi()->popupManager()->doPopup(account(), PopupManager::AlertGcHighlight, jid,
                                                          dm.from().resource(), &i, e);
            }
        }
    }

    if (from.isEmpty()) {
        auto mv = MessageView::systemMessage(dm.body());
        mv.setDateTime(dm.timeStamp());
        dispatchMessage(mv);
    } else
        appendMessage(m, d->alert);
}

void GCMainDlg::joined()
{
    if (d->state == Private::Connecting) {
        d->usersModel->clear();
        d->state = Private::Connected;
        d->actions->action("gchat_set_topic")->setEnabled(true);
        setStatusTabIcon(STATUS_ONLINE);
        ui_.mle->chatEdit()->setEnabled(true);
        setConnecting();
        appendSysMsg(tr("Connected."), true);
    }
    account()->addMucItem(d->dlg->jid().bare());
}

void GCMainDlg::setPassword(const QString &p) { d->password = p; }

const QString &GCMainDlg::nick() const { return d->self; }

const QDateTime &GCMainDlg::lastMsgTime() const { return d->te_log()->lastMsgTime(); }

bool GCMainDlg::isLastMessageAlert() const { return d->alert; }

void GCMainDlg::appendSysMsg(const QString &str, bool alert)
{
    MessageView mv = MessageView::systemMessage(str);
    mv.setAlert(alert && PsiOptions::instance()->getOption(QStringLiteral("options.ui.muc.use-highlighting")).toBool());
    dispatchMessage(mv);
}

void GCMainDlg::dispatchMessage(const MessageView &mv)
{
    if (d->trackBar && !mv.isLocal() && !mv.isSpooled() && mv.reactionsId().isEmpty())
        d->doTrackBar();

    ui_.log->dispatchMessage(mv);
    if (mv.isAlert())
        doAlert();
}

void GCMainDlg::appendMessage(const Message &m, bool alert)
{
    Message dm = m.displayMessage();
    // figure out the encryption state
    bool encChanged = false;
    bool encEnabled = false;
    {
        if (lastWasEncrypted_ != dm.wasEncrypted()) {
            encChanged = true;
        }
        lastWasEncrypted_ = dm.wasEncrypted();
        encEnabled        = lastWasEncrypted_;
    }
    if (encChanged) {
        ui_.log->setEncryptionEnabled(encEnabled);
        QString msg = QString("<icon name=\"psi/cryptoNo\"> ") + tr("Encryption is disabled");
        if (encEnabled) {
            if (!dm.encryptionProtocol().isEmpty()) {
                msg = QString("<icon name=\"psi/cryptoYes\"> ")
                    + tr("%1 encryption is enabled").arg(dm.encryptionProtocol());
            } else {
                msg = QString("<icon name=\"psi/cryptoYes\"> ") + tr("Encryption is enabled");
            }
        }
        dispatchMessage(MessageView::fromHtml(msg, MessageView::System));
    }

    MessageView mv(MessageView::Message);
    if (dm.containsHTML() && PsiOptions::instance()->getOption("options.html.muc.render").toBool()
        && !dm.html().text().isEmpty()) {
        mv.setHtml(dm.html().toString("span"));
    } else {
        mv.setPlainText(dm.body());
    }
    if (!PsiOptions::instance()->getOption("options.ui.muc.use-highlighting").toBool())
        alert = false;
    mv.setMessageId(dm.id());
    mv.setAlert(alert);
    mv.setUserId(dm.from().full()); // theoretically, this can be inferred from the chat dialog properties
    mv.setNick(dm.from().resource());
    mv.setLocal(mv.nick() == d->self);
    mv.setSpooled(dm.spooled());
    mv.setDateTime(dm.timeStamp());
    mv.setReplaceId(dm.replaceId());
    account()->psi()->fileSharingManager()->fillMessageView(mv, dm, account());

    dispatchMessage(mv);

    // if we're not active, notify the user by changing the title
    if (!isActiveTab() && !mv.isLocal()) {
        ++d->pending;
        if (alert)
            ++d->hPending;
        UserListItem *u = account()->find(d->dlg->jid().bare());
        if (u) {
            u->setPending(d->pending, d->hPending);
            account()->updateEntry(*u);
        }
        invalidateTab();
    }

    // if the message spoke to us, alert the user before closing this window
    // except that keepopen doesn't seem to be implemented for this class yet.
    /*if(alert) {
        d->keepOpen = true;
        QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
                }*/
    emit messageAppended(dm.body(), ui_.log->textWidget());
}

void GCMainDlg::doAlert()
{
    if (!isActiveTab())
        if (PsiOptions::instance()->getOption("options.ui.flash-windows").toBool())
            doFlash(true);
}

const QString &GCMainDlg::getDisplayName() const { return d->mucName; }

QString GCMainDlg::desiredCaption() const
{
    QString cap = "";

    if (d->pending > 0) {
        cap += "* ";
        if (d->pending > 1) {
            cap += QString("[%1] ").arg(d->pending);
        }
    }
    cap += getDisplayName();

    return cap;
}

void GCMainDlg::setLooks()
{
    const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
    if (!css.isEmpty()) {
        setStyleSheet(css);
        d->mle()->setCssString(css);
    }
    ui_.vsplitter->optionsChanged();

    // update the fonts
    QFont f;
    f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());
    ui_.log->setFont(f);
    ui_.mle->chatEdit()->setFont(f);

    f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.contactlist").toString());
    ui_.lv_users->setFont(f);

    if (PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m1.visible").toBool()) {
        ui_.toolbar->show();
        // ui_.tb_actions->hide();
        ui_.tb_emoticons->hide();
    } else {
        ui_.toolbar->hide();
        ui_.tb_emoticons->setVisible(PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool());
        ui_.tb_actions->show();
    }

    if (PsiOptions::instance()->getOption("options.ui.disable-send-button").toBool()) {
        ui_.pb_send->hide();
    } else {
        ui_.pb_send->show();
    }

    setWindowOpacity(double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))
                     / 100);

    // update the widget icon
#ifndef Q_OS_MAC
    setWindowIcon(IconsetFactory::icon("psi/start-chat").icon());
#endif

    ui_.lv_users->setVerticalScrollBarPolicy(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.disable-scrollbar").toBool()
            ? Qt::ScrollBarAlwaysOff
            : Qt::ScrollBarAsNeeded);
    ui_.lv_users->setLooks();
    setMucSelfAvatar();
    updateIdentityVisibility();
}

void GCMainDlg::updateIdentityVisibility()
{
    if (!PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool()) {
        bool visible = account()->psi()->contactList()->enabledAccounts().count() > 1;
        ui_.lb_ident->setVisible(visible);
    } else {
        ui_.lb_ident->setVisible(false);
    }
}

void GCMainDlg::setToolbuttons()
{
    ui_.toolbar->clear();
    PsiOptions *options      = PsiOptions::instance();
    QStringList actionsNames = options->getOption("options.ui.contactlist.toolbars.m1.actions").toStringList();
    for (const QString &actionName : actionsNames) {
#ifdef PSI_PLUGINS
        if (actionName.endsWith("-plugin")) {
            QString name = actionName.mid(0, actionName.length() - 7);
            if (!name.isEmpty())
                PluginManager::instance()->addGCToolBarButton(this, ui_.toolbar, account(), jid().full(), name);
            continue;
        }
#endif

        // Hack. separator action can be added only once.
        if (actionName == "separator") {
            ui_.toolbar->addSeparator();
            continue;
        }

        auto action = d->actions->action(actionName);
        if (action) {
            action->addTo(ui_.toolbar);
            if (actionName == "gchat_icon" || actionName == "gchat_templates") {
                static_cast<QToolButton *>(ui_.toolbar->widgetForAction(action))
                    ->setPopupMode(QToolButton::InstantPopup);
            }
        }
    }
}

void GCMainDlg::optionsUpdate()
{
    /*QMimeSourceFactory *m = ui_.log->mimeSourceFactory();
    ui_.log->setMimeSourceFactory(PsiIconset::instance()->emoticons.generateFactory());
    delete m;*/

    setLooks();
    setToolbuttons();
    setShortcuts();
    d->typeahead->optionsUpdate();
    // update status icons
    d->usersModel->updateAll();
}

void GCMainDlg::lv_action(const QString &nick, const Status &s, int x)
{
    if (x == 0) {
        account()->invokeGCMessage(jid().withResource(nick));
    } else if (x == 1) {
        account()->invokeGCChat(jid().withResource(nick));
    } else if (x == 2) {
        UserListItem u;
        u.setJid(jid().withResource(nick));
        u.setName(nick);

        // make a resource so the contact appears online
        UserResource ur;
        ur.setName(nick);
        ur.setStatus(s);
        u.userResourceList().append(ur);

        StatusShowDlg *w = new StatusShowDlg(u);
        w->show();
    } else if (x == 3) {
        account()->invokeGCInfo(jid().withResource(nick));
    } else if (x == 4) {
        account()->sendFiles(jid().withResource(nick));
    } else if (x == 5) {
        account()->actionVoice(jid().withResource(nick));
    } else if (x == 6) {
        account()->actionExecuteCommandSpecific(jid().withResource(nick));
    } else if (x == 10) {
        d->mucManager->kick(nick);
    } else if (x == 11) {
        auto contact = d->usersModel->findEntry(nick);
        d->mucManager->ban(contact->status.mucItem().jid());
    } else if (x > 11 && x < 19) {
        MUCReasonsEditor editor(this);
        QString          reason;
        if (editor.exec())
            reason = editor.reason();
        else
            return;
        auto contact = d->usersModel->findEntry(nick);
        if (!contact)
            return;
        switch (x) {
        case 12:
            if (contact->status.mucItem().role() != MUCItem::Visitor)
                d->mucManager->setRole(nick, MUCItem::Visitor, reason);
            break;
        case 13:
            if (contact->status.mucItem().role() != MUCItem::Participant)
                d->mucManager->setRole(nick, MUCItem::Participant, reason);
            break;
        case 14:
            if (contact->status.mucItem().role() != MUCItem::Moderator)
                d->mucManager->setRole(nick, MUCItem::Moderator, reason);
            break;
        case 15:
            if (contact->status.mucItem().affiliation() != MUCItem::NoAffiliation)
                d->mucManager->setAffiliation(contact->status.mucItem().jid(), MUCItem::NoAffiliation, reason);
            break;
        case 16:
            if (contact->status.mucItem().affiliation() != MUCItem::Member)
                d->mucManager->setAffiliation(contact->status.mucItem().jid(), MUCItem::Member, reason);
            break;
        case 17:
            if (contact->status.mucItem().affiliation() != MUCItem::Admin)
                d->mucManager->setAffiliation(contact->status.mucItem().jid(), MUCItem::Admin, reason);
            break;
        case 18:
            if (contact->status.mucItem().affiliation() != MUCItem::Owner)
                d->mucManager->setAffiliation(contact->status.mucItem().jid(), MUCItem::Owner, reason);
            break;
        }
    } else if (x >= 100 && x < 300) {
        // Kick || Ban with reason
        QString     reason;
        QStringList reasons = PsiOptions::instance()->getOption("options.muc.reasons").toStringList();
        if (x == 100 || x == 200) {
            // Show custom reason dialog
            MUCReasonsEditor *editor = new MUCReasonsEditor(this);
            if (editor->exec())
                reason = editor->reason();
            delete editor;
        } else {
            int idx = (x < 200) ? x - 101 : x - 201;
            if (idx < reasons.count())
                reason = reasons[idx];
        }
        if (!reason.isEmpty()) {
            if (x < 200)
                d->mucManager->kick(nick, reason);
            else {
                auto contact = d->usersModel->findEntry(nick);
                if (!contact)
                    return;
                d->mucManager->ban(contact->status.mucItem().jid(), reason);
            }
        }
    }
}

void GCMainDlg::contextMenuEvent(QContextMenuEvent *) { d->pm_settings->exec(QCursor::pos()); }

void GCMainDlg::buildMenu()
{
    // Dialog menu
    d->pm_settings->clear();

    d->pm_settings->addAction(d->actions->action("gchat_info"));
    d->pm_settings->addAction(d->actions->action("gchat_clear"));
    d->pm_settings->addAction(d->actions->action("gchat_configure"));
    // #ifdef WHITEBOARDING
    //     d->act_whiteboard->addTo( d->pm_settings );
    // #endif
    d->pm_settings->addSeparator();

    d->pm_settings->addAction(d->actions->action("gchat_icon"));
    d->pm_settings->addAction(d->actions->action("gchat_templates"));
    d->pm_settings->addAction(d->act_pastesend);
    d->pm_settings->addAction(d->act_nick);
    d->pm_settings->addAction(d->act_bookmark);
    d->pm_settings->addAction(d->actions->action("gchat_set_topic"));
    if (PsiOptions::instance()->getOption("options.ui.tabs.multi-rows").toBool() && d->tabmode) {
        d->pm_settings->addSeparator();
        d->pm_settings->addAction(d->actions->action("gchat_pin_tab"));
    }

#ifdef PSI_PLUGINS
    if (!PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m1.visible").toBool()) {
        d->pm_settings->addSeparator();
        PluginManager::instance()->addGCToolBarButton(this, d->pm_settings, account(), jid().full());
    }
#endif
    d->pm_settings->addAction(d->actions->action("gchat_share_files"));
}

void GCMainDlg::chatEditCreated()
{
    d->mCmdSite.setInput(ui_.mle->chatEdit());
    d->mCmdSite.setPrompt(ui_.mini_prompt);
    d->tabCompletion.setTextEdit(d->mle());

    ui_.log->setDialog(this);
    ui_.mle->chatEdit()->setDialog(this);

    ui_.mle->chatEdit()->installEventFilter(d);
    connect(ui_.mle->chatEdit(), &ChatEdit::fileSharingRequested, this, [this](const QMimeData *data) {
        FileShareDlg::shareFiles(
            account(), jid().withResource(d->self), data,
            [this](const QList<XMPP::Reference> &&rl, const QString &desc) { d->doFileShare(std::move(rl), desc); },
            this);
    });
}

TabbableWidget::State GCMainDlg::state() const
{
    return d->hPending ? TabbableWidget::State::Highlighted : TabbableWidget::State::None;
}

int GCMainDlg::unreadMessageCount() const { return d->pending; }

void GCMainDlg::setStatusTabIcon(int status) { setTabIcon(PsiIconset::instance()->statusPtr(jid(), status)->icon()); }

void GCMainDlg::resizeEvent(QResizeEvent *e)
{
    TabbableWidget::resizeEvent(e);

    if (!isVisible())
        return;

    QList<int> sizes       = ui_.hsplitter->sizes();
    bool       leftRoster  = PsiOptions::instance()->getOption("options.ui.muc.roster-at-left").toBool();
    int        logWidth    = !leftRoster ? sizes.first() : sizes.last();
    int        rosterWidth = leftRoster ? sizes.first() : sizes.last();
    int        dw          = rosterWidth - d->rosterSize;
    sizes.clear();
    sizes.append(logWidth + dw);
    if (leftRoster) {
        sizes.prepend(d->rosterSize);
    } else {
        sizes.append(d->rosterSize);
    }

    ui_.hsplitter->setSizes(sizes);
    QTimer::singleShot(0, this, SLOT(horizSplitterMoved()));
}

QStringList GCMainDlg::mucRosterContent() const { return d->usersModel->nickList(); }

void GCMainDlg::sendButtonMenu()
{
    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        menu->setParams(true);
        menu->exec(QCursor::pos());
        menu->setParams(false);
        d->mle()->setFocus();
    }
}

void GCMainDlg::editTemplates()
{
    if (TabbableWidget::isActiveTab()) {
        showTemplateEditor();
    }
}

void GCMainDlg::doPasteAndSend()
{
    if (TabbableWidget::isActiveTab()) {
        d->mle()->paste();
        mle_returnPressed();
        d->act_pastesend->setEnabled(false);
        QTimer::singleShot(2000, this, SLOT(psButtonEnabled()));
    }
}

void GCMainDlg::psButtonEnabled() { d->act_pastesend->setEnabled(true); }

void GCMainDlg::sendTemp(const QString &templText)
{
    if (TabbableWidget::isActiveTab()) {
        if (!templText.isEmpty()) {
            d->mle()->textCursor().insertText(templText);
            if (!PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool())
                mle_returnPressed();
        }
    }
}

#include "groupchatdlg.moc"
