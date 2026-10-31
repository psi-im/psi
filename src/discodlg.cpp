/*
 * discodlg.cpp - main dialog for the Service Discovery protocol
 * Copyright (C) 2003  Michail Pishchagin
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

#include "discodlg.h"
#include "discoitems_p.h"

#include "accountlabel.h"
#include "bookmarkmanager.h"
#include "busywidget.h"
#include "common.h"
#include "iconaction.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_tasks.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "stretchwidget.h"
#include "tasklist.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalMapper>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>

//----------------------------------------------------------------------------

PsiIcon category2icon(PsiAccount *acc, const Jid &jid, const QString &category, const QString &type,
                      int status = STATUS_ONLINE)
{
    // TODO: update this to https://xmpp.org/registrar/disco-categories.html#gateway

    // still have to add more options...
    if (category == "service" || category == "gateway") {
        QString trans;

        if (type == "facebook")
            trans = "facebook";
        else if (type == "vkontakte")
            trans = "vkontakte";
        else if (type == "gadu-gadu" || type == "x-gadugadu")
            trans = "gadugadu";
        else if (type == "sms")
            trans = "sms";
        else
            trans = "transport";

        return PsiIconset::instance()->transportStatus(trans, status);

        // irc
        // jud
        // pager
        // jabber
        // serverlist
        // smtp
    } else if (category == "conference") {
        if (type == "public" || type == "private" || type == "text" || type == "irc") {
            PsiIcon icon = IconsetFactory::icon("psi/groupChat");
            if (acc->bookmarkManager()->isBookmarked(jid)) {
                static QImage img;
                if (img.isNull()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    const auto fontSize = qApp->fontMetrics().height();
#else
                    const auto fontSize = QFontMetrics(qApp->font()).height();
#endif
                    auto is = int(fontSize * EqTextIconK + .5);
                    img     = icon.pixmap(QSize(is, is)).toImage();
                    QPainter p(&img);
                    PsiIcon  bicon    = IconsetFactory::icon("psi/bookmark_remove");
                    auto     ovrlRect = QRect(img.rect().center(), img.rect().bottomRight());
                    p.drawImage(ovrlRect, bicon.image(ovrlRect.size()));
                }
                icon.setImpix(Impix(img));
            }
            return icon;
        } else if (type == "url")
            return IconsetFactory::icon("psi/www");
        // irc
        // list
        // topic
    } else if (category == "validate") {
        if (type == "xml")
            return IconsetFactory::icon("psi/xml");
        // grammar
        // spell
    } else if (category == "user" || category == "client") {
        // client
        // forward
        // inbox
        // portable
        // voice
        return PsiIconset::instance()->status(STATUS_ONLINE);
    }
    // application
    // bot
    // calendar
    // editor
    // fileserver
    // game
    // whiteboard
    // headline
    // logger
    // notice
    // rss
    // stock
    // keyword
    // dictionary
    // dns
    // software
    // thesaurus
    // web
    // whois
    // render
    // en2ru
    // ??2??
    // tts
    return PsiIcon();
}

//----------------------------------------------------------------------------
// DiscoData -- a shared data struct
//----------------------------------------------------------------------------

static QIcon discoItemIcon(PsiAccount *pa, const DiscoItem &item, bool error)
{
    const int status = error ? STATUS_ERROR : STATUS_ONLINE;
    if (!item.identities().isEmpty()) {
        const auto &id = item.identities().first();
        if (!id.category.isEmpty()) {
            auto icon = category2icon(pa, item.jid(), id.category, id.type, status).icon();
            if (!icon.isNull())
                return icon;
        }
    }
    return PsiIconset::instance()->status(item.jid(), status).icon();
}

//----------------------------------------------------------------------------
// DiscoList
//----------------------------------------------------------------------------

class DiscoListView : public QTreeWidget {
    Q_OBJECT

    DiscoDlg *dlg;

public:
    DiscoListView(DiscoDlg *parent);

public slots:
    void updateItemsVisibility(const QString &filter);

protected:
    bool maybeTip(const QPoint &);

    // reimplemented
    bool eventFilter(QObject *o, QEvent *e);
    void resizeEvent(QResizeEvent *);
};

DiscoListView::DiscoListView(DiscoDlg *parent) : QTreeWidget(parent)
{
    dlg = parent;
    installEventFilter(this);
    setHeaderLabels(QStringList() << tr("Name") << tr("JID") << tr("Node"));
    //    header()->setResizeMode(0, QHeaderView::Stretch);
    //    header()->setResizeMode(1, QHeaderView::ResizeToContents);
    //    header()->setResizeMode(2, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    sortByColumn(1, Qt::AscendingOrder);
}

void DiscoListView::resizeEvent(QResizeEvent *e)
{
    QTreeWidget::resizeEvent(e);

    QHeaderView *h = header();
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    h->resizeSection(2, h->fontMetrics().horizontalAdvance(headerItem()->text(2)) * 2);
#else
    h->resizeSection(2, h->fontMetrics().width(headerItem()->text(2)) * 2);
#endif
    float remainingWidth = viewport()->width() - h->sectionSize(2);
    h->resizeSection(1, int(remainingWidth * 0.3f));
    h->resizeSection(0, int(remainingWidth * 0.7f));

    // h->adjustHeaderSize();
}

/**
 * \param pos should be in global coordinate system.
 */
bool DiscoListView::maybeTip(const QPoint &pos)
{
    QTreeWidgetItem *twi = itemAt(viewport()->mapFromGlobal(pos));
    if (!twi || twi->type() == MoreItemsType)
        return false;

    DiscoListItem *i = static_cast<DiscoListItem *>(twi);

    QStringList identityIcons;
    for (const auto &id : i->item().identities())
        identityIcons << category2icon(dlg->account(), i->item().jid(), id.category, id.type).name();
    const auto text = discoItemToolTip(i->item(), i->getErrorInfo(), identityIcons);
    //    QRect r( visualItemRect(i) );
    PsiToolTip::showText(pos, text, this);
    return true;
}

bool DiscoListView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::ToolTip && o->isWidgetType()) {
        QWidget    *w  = static_cast<QWidget *>(o);
        QHelpEvent *he = static_cast<QHelpEvent *>(e);
        maybeTip(w->mapToGlobal(he->pos()));
        return true;
    }
    return QTreeWidget::eventFilter(o, e);
}

// returns false if parent item should not be hidden (it has visible children)
static bool updateItemsRecursively(QTreeWidgetItem *parent, const QString &filter)
{
    bool hidden = true;
    for (int j = 0; j < parent->childCount(); j++) {
        QTreeWidgetItem *i = parent->child(j);
        if (filter.isEmpty()) {
            i->setHidden(false);
            updateItemsRecursively(i, filter);
        } else {
            bool v = true;
            if (i->isExpanded()) {
                v = updateItemsRecursively(i, filter);
            }
            v &= !(i->text(0).contains(filter, Qt::CaseInsensitive)
                   || i->text(1).contains(filter, Qt::CaseInsensitive));
            i->setHidden(v);
            hidden &= v;
        }
    }
    return hidden;
}

void DiscoListView::updateItemsVisibility(const QString &filter)
{
    for (int n = 0; n < topLevelItemCount(); n++) {
        QTreeWidgetItem *it = topLevelItem(n);
        updateItemsRecursively(it, filter);
    }
}

//----------------------------------------------------------------------------
// DiscoDlg::Private
//----------------------------------------------------------------------------

class DiscoDlg::Private : public QObject {
    Q_OBJECT

private:
    // helper class to store browser history
    class History {
    public:
        History(const XMPP::Jid &_jid, const QString &_node) : jid(_jid), node(_node) { }

        XMPP::Jid jid;
        QString   node;
    };

public: // data
    DiscoDlg *dlg;
    Jid       jid;
    QString   node;

    PsiAccount *pa;
    DiscoData   data;

    QToolBar   *toolBar;
    IconAction *actBrowse, *actBack, *actForward, *actRefresh, *actStop;

    // custom actions, that will be added to toolbar and context menu
    IconAction *actRegister, *actUnregister, *actSearch, *actJoin, *actAHCommand, *actVCard, *actAdd, *actQueryVersion;

    typedef QList<History *> HistoryList;
    HistoryList              backHistory, forwardHistory;

    BusyWidget *busy;

    int itemsPerPage;

    QString addressLastText;
    QString nodeLastText;

public: // functions
    Private(DiscoDlg *parent, PsiAccount *pa);
    ~Private();

public slots:
    void doDiscoUserInput();
    void doDisco(QString host = QString(), QString node = QString(), bool doHistory = true);

    void actionStop();
    void actionRefresh();
    void actionBrowse();

    void actionBack();
    void actionForward();
    void updateBackForward();

    void updateComboBoxes(Jid j, QString node);

    void itemUpdateStarted();
    void itemUpdateFinished();

    void disableButtons();
    void enableButtons(const DiscoItem &);

    void itemSelected(QTreeWidgetItem *);
    void itemExpanded(QTreeWidgetItem *);
    void itemClicked(QTreeWidgetItem *);
    void itemDoubleclicked(QTreeWidgetItem *);
    bool eventFilter(QObject *, QEvent *);

    // features...
    void actionActivated(int);

    void objectDestroyed(QObject *);

private:
    friend class DiscoListItem;
};

DiscoDlg::Private::Private(DiscoDlg *parent, PsiAccount *pa)
{
    dlg                   = parent;
    this->pa              = pa;
    data.rootTask         = pa->client()->rootTask();
    data.autoItemsEnabled = [parent] { return parent->ck_autoItems->isChecked(); };
    data.autoInfoEnabled  = [parent] { return parent->ck_autoInfo->isChecked(); };
    data.itemsPerPage     = [parent] { return parent->itemsPerPage(); };
    data.itemIcon         = [pa](const DiscoItem &item, bool error) { return discoItemIcon(pa, item, error); };
    data.showError        = [parent](const QString &message) { QMessageBox::critical(parent, tr("Error"), message); };
    data.tasks            = new TaskList;
    connect(data.tasks, SIGNAL(started()), SLOT(itemUpdateStarted()));
    connect(data.tasks, SIGNAL(finished()), SLOT(itemUpdateFinished()));
    data.d = new DiscoConnector(this);
    connect(data.d, SIGNAL(itemUpdated(QTreeWidgetItem *)), SLOT(itemSelected(QTreeWidgetItem *)));

    // mess with widgets
    busy = parent->busy;
    connect(busy, SIGNAL(destroyed(QObject *)), SLOT(objectDestroyed(QObject *)));

    QTreeWidget *lv_discoOld = dlg->lv_disco;
    dlg->lv_disco            = new DiscoListView(dlg);
    replaceWidget(lv_discoOld, dlg->lv_disco);

    dlg->lv_disco->installEventFilter(this);
    dlg->le_filter->installEventFilter(this);
    connect(dlg->lv_disco, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            SLOT(itemSelected(QTreeWidgetItem *)));
    connect(dlg->lv_disco, SIGNAL(itemExpanded(QTreeWidgetItem *)), SLOT(itemExpanded(QTreeWidgetItem *)));
    connect(dlg->lv_disco, SIGNAL(itemClicked(QTreeWidgetItem *, int)), SLOT(itemClicked(QTreeWidgetItem *)));
    connect(dlg->lv_disco, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
            SLOT(itemDoubleclicked(QTreeWidgetItem *)));
    connect(dlg->le_filter, SIGNAL(textChanged(QString)), dlg->lv_disco, SLOT(updateItemsVisibility(QString)));

    // create actions
    actBrowse = new IconAction(tr("Browse"), "psi/jabber", tr("&Browse"), 0, dlg);
    connect(actBrowse, SIGNAL(triggered()), SLOT(actionBrowse()));
    actRefresh = new IconAction(tr("Refresh Item"), "psi/reload", tr("&Refresh Item"), 0, dlg);
    connect(actRefresh, SIGNAL(triggered()), SLOT(actionRefresh()));
    actStop = new IconAction(tr("Stop"), "psi/stop", tr("Sto&p"), 0, dlg);
    connect(actStop, SIGNAL(triggered()), SLOT(actionStop()));
    actBack = new IconAction(tr("Back"), "psi/arrowLeft", tr("&Back"), 0, dlg);
    connect(actBack, SIGNAL(triggered()), SLOT(actionBack()));
    actForward = new IconAction(tr("Forward"), "psi/arrowRight", tr("&Forward"), 0, dlg);
    connect(actForward, SIGNAL(triggered()), SLOT(actionForward()));

    // custom actions
    actRegister = new IconAction(tr("Register"), "psi/register", tr("&Register"), 0, dlg);
    connect(actRegister, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_Register); });

    actUnregister = new IconAction(tr("Unregister"), "psi/cancel", tr("&Unregister"), 0, dlg);
    connect(actUnregister, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_Gateway); });

    actSearch = new IconAction(tr("Search"), "psi/search", tr("&Search"), 0, dlg);
    connect(actSearch, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_Search); });

    actJoin = new IconAction(tr("Join"), "psi/groupChat", tr("&Join"), 0, dlg);
    connect(actJoin, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_Groupchat); });

    actAHCommand = new IconAction(tr("Execute command"), "psi/command", tr("&Execute command"), 0, dlg);
    connect(actAHCommand, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_AHCommand); });

    actVCard = new IconAction(tr("vCard"), "psi/vCard", tr("&vCard"), 0, dlg);
    connect(actVCard, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_VCard); });

    actAdd = new IconAction(tr("Add to roster"), "psi/addContact", tr("&Add to roster"), 0, dlg);
    connect(actAdd, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_Add); });

    actQueryVersion = new IconAction(tr("Query version"), "psi/info", tr("&Query version"), 0, dlg);
    connect(actQueryVersion, &IconAction::triggered, this, [this]() { actionActivated(Features::FID_QueryVersion); });

    // create toolbar
    toolBar = new QToolBar(tr("Service Discovery toolbar"), dlg);
    int s   = PsiIconset::instance()->system().iconSize();
    toolBar->setIconSize(QSize(s, s));
    toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    replaceWidget(dlg->toolBarPlaceholder, toolBar);

    toolBar->addAction(actBack);
    toolBar->addAction(actBrowse);
    toolBar->addAction(actForward);

    toolBar->addSeparator();
    toolBar->addAction(actRefresh);
    toolBar->addAction(actStop);

    // custom actions
    toolBar->addSeparator();
    toolBar->addAction(actRegister);
    toolBar->addAction(actUnregister);
    toolBar->addAction(actSearch);
    toolBar->addAction(actJoin);

    toolBar->addSeparator();
    toolBar->addAction(actAdd);
    toolBar->addAction(actVCard);
    toolBar->addAction(actAHCommand);
    toolBar->addAction(actQueryVersion);

    toolBar->addWidget(new StretchWidget(toolBar));
    AccountLabel *lb_ident = new AccountLabel(toolBar);
    lb_ident->setAccount(pa);
    lb_ident->setShowJid(false);
    toolBar->addWidget(lb_ident);

    // misc stuff
    disableButtons();
    actStop->setEnabled(false); // stop action is not handled by disableButtons()
    updateBackForward();        // same applies to back & forward
}

DiscoDlg::Private::~Private()
{
    qDeleteAll(backHistory);
    qDeleteAll(forwardHistory);
    data.cancelTasks();
    delete data.tasks;
}

void DiscoDlg::Private::doDiscoUserInput()
{
    addressLastText = dlg->cb_address->currentText();
    nodeLastText    = dlg->cb_node->currentText().trimmed();
    doDisco(addressLastText, nodeLastText, true);
}

void DiscoDlg::Private::doDisco(QString _host, QString _node, bool doHistory)
{
    if (!pa->checkConnected(dlg))
        return;

    // Strip whitespace
    Jid     j;
    QString host = _host;
    j            = host.trimmed();
    if (!j.isValid())
        return;

    QString n = _node.trimmed();
    // check, whether we need to update history
    if ((jid.full() != j.full()) || (node != n)) {
        if (doHistory) {
            backHistory.append(new History(jid, node));
            qDeleteAll(forwardHistory);
            forwardHistory.clear();
        }
    }

    jid  = j;
    node = n;

    updateComboBoxes(jid, node);

    data.cancelTasks();
    disableButtons();
    updateBackForward();

    dlg->lv_disco->clear();

    // create new root item
    DiscoItem di;
    di.setJid(jid);
    di.setNode(node);

    DiscoListItem *root = new DiscoListItem(di, &data, dlg->lv_disco);
    root->setHidden(false); // don't confuse users with empty root

    //    DiscoListItem::setExpanded(true); will be called from
    //    DiscoDlg::Private::itemExpanded(QTreeWidgetItem *item)
    //    So we preventing disco#items request sends twice
    static_cast<QTreeWidgetItem *>(root)->setExpanded(true); // begin browsing;
}

void DiscoDlg::Private::updateComboBoxes(Jid j, QString n)
{
    dlg->cb_address->disconnect(this);
    dlg->cb_node->disconnect(this);
    dlg->cb_address->lineEdit()->disconnect(this);
    dlg->cb_node->lineEdit()->disconnect(this);

    pa->psi()->recentBrowseAdd(j.full());
    dlg->cb_address->clear();
    dlg->cb_address->addItems(pa->psi()->recentBrowseList());

    pa->psi()->recentNodeAdd(n);
    dlg->cb_node->clear();
    dlg->cb_node->addItems(pa->psi()->recentNodeList());

    dlg->connectAddressNodeSignals();
}

void DiscoDlg::Private::actionStop() { data.cancelTasks(); }

void DiscoDlg::Private::actionRefresh()
{
    DiscoListItem *it = DiscoListItem::fromItem(dlg->lv_disco->currentItem());
    if (!it)
        return;

    it->updateItems();
    it->updateInfo();
}

void DiscoDlg::Private::actionBrowse()
{
    DiscoListItem *it = DiscoListItem::fromItem(dlg->lv_disco->currentItem());
    if (!it)
        return;

    doDisco(it->item().jid().full(), it->item().node());
}

void DiscoDlg::Private::actionBack()
{
    forwardHistory.append(new History(jid, node));
    History *h = backHistory.takeLast();
    doDisco(h->jid.full(), h->node, false);
    delete h;
}

void DiscoDlg::Private::actionForward()
{
    backHistory.append(new History(jid, node));
    History *h = forwardHistory.takeLast();
    doDisco(h->jid.full(), h->node, false);
    delete h;
}

void DiscoDlg::Private::updateBackForward()
{
    actBack->setEnabled(!backHistory.isEmpty());
    actForward->setEnabled(!forwardHistory.isEmpty());
}

void DiscoDlg::Private::itemUpdateStarted()
{
    actStop->setEnabled(true);
    if (busy)
        busy->start();
}

void DiscoDlg::Private::itemUpdateFinished()
{
    actStop->setEnabled(false);
    if (busy)
        busy->stop();
}

void DiscoDlg::Private::disableButtons()
{
    DiscoItem di;
    enableButtons(di);
}

void DiscoDlg::Private::enableButtons(const DiscoItem &it)
{
    bool itemSelected = !it.jid().full().isEmpty();
    actRefresh->setEnabled(itemSelected);
    actBrowse->setEnabled(itemSelected);

    // custom actions
    Features f = it.features();
    actRegister->setEnabled(f.hasRegister());
    actUnregister->setEnabled(f.hasRegister());
    actSearch->setEnabled(f.hasSearch());
    actJoin->setEnabled(f.hasGroupchat());
    actAdd->setEnabled(itemSelected);
    actVCard->setEnabled(f.hasVCard());
    actAHCommand->setEnabled(f.hasCommand());
    actQueryVersion->setEnabled(f.hasVersion());
}

void DiscoDlg::Private::itemSelected(QTreeWidgetItem *item)
{
    DiscoBaseItem *it = static_cast<DiscoBaseItem *>(item);
    if (!it) {
        disableButtons();
        return;
    }

    it->itemSelected();

    const DiscoItem di = it->item();
    enableButtons(di);
}

void DiscoDlg::Private::itemExpanded(QTreeWidgetItem *item)
{
    DiscoListItem *it = DiscoListItem::fromItem(item);
    if (it)
        it->setExpanded(true);
}

void DiscoDlg::Private::itemClicked(QTreeWidgetItem *item)
{
    DiscoBaseItem *it = static_cast<DiscoBaseItem *>(item);
    if (it)
        it->itemClicked();
}

void DiscoDlg::Private::itemDoubleclicked(QTreeWidgetItem *item)
{
    if (!item || item->type() == MoreItemsType)
        return;

    DiscoListItem  *it = static_cast<DiscoListItem *>(item);
    const DiscoItem d  = it->item();
    const Features &f  = d.features();

    // set the prior state of item
    // FIXME: causes minor flickering
    long id = 0;

    // trigger default action
    if (f.hasCommand()) {
        id = Features::FID_AHCommand;
    }
    if (!d.identities().isEmpty()) {
        // FIXME: check the category and type for JUD!
        DiscoItem::Identity ident       = d.identities().first();
        bool                searchFirst = ident.category == "service" && ident.type == "jud";

        if (searchFirst && f.hasSearch()) {
            id = Features::FID_Search;
        } else if (ident.category == "conference" && f.hasGroupchat()) {
            id = Features::FID_Groupchat;
        } else if (f.hasRegister()) {
            id = Features::FID_Register;
        }
    }

    if (id > 0) {
        if (!it->isExpanded()) {
            if (it->childCount())
                it->setExpanded(true);
        } else {
            it->setExpanded(false);
        }
        emit dlg->featureActivated(Features::feature(id), d.jid(), d.node());
    }
}

bool DiscoDlg::Private::eventFilter(QObject *object, QEvent *event)
{
    if (object == dlg->lv_disco) {
        if (event->type() == QEvent::ContextMenu) {
            auto e = static_cast<QContextMenuEvent *>(event);

            QTreeWidgetItem *twi = dlg->lv_disco->currentItem();
            if (!twi || twi->type() == MoreItemsType)
                return true;
            DiscoListItem *it = static_cast<DiscoListItem *>(twi);

            // prepare features list
            QList<long> idFeatures;
            QStringList features = it->item().features().list();
            { // convert all features to their IDs
                QStringList::Iterator it = features.begin();
                for (; it != features.end(); ++it) {
                    Features f(*it);
                    if (f.id() > Features::FID_None)
                        idFeatures.append(Features::id(*it));
                }
                // qHeapSort(idFeatures);
            }

            QList<long> ids;
            { // ensure, that there's in no duplicated IDs inside. FIXME: optimize this, anyone?
                long                  id = 0, count = 0;
                QList<long>::Iterator it;
                while (count < long(idFeatures.count())) {
                    bool found = false;

                    for (it = idFeatures.begin(); it != idFeatures.end(); ++it) {
                        if (id == *it) {
                            if (!found) {
                                found = true;
                                ids.append(id);
                            }
                            count++;
                        }
                    }
                    id++;
                }
            }

            // prepare popup menu
            QMenu p;

            actBrowse->addTo(&p);
            actRefresh->addTo(&p);
            actStop->addTo(&p);

            // custom actions
            p.addSeparator();
            actRegister->addTo(&p);
            actUnregister->addTo(&p);
            actSearch->addTo(&p);
            actJoin->addTo(&p);

            p.addSeparator();
            actAdd->addTo(&p);
            actVCard->addTo(&p);
            actAHCommand->addTo(&p);
            actQueryVersion->addTo(&p);

            // popup with all available features
            QMenu                *fm = new QMenu(&p);
            QHash<QAction *, int> actions;
            {
                QList<long>::Iterator it = ids.begin();
                for (; it != ids.end(); ++it)
                    actions.insert(fm->addAction(Features::name(*it)), int(*it) + 10000); // TODO: add pixmap
            }

            // p.insertSeparator();
            // int menuId = p.insertItem(tr("Activate &Feature"), fm);
            // p.setItemEnabled(menuId, !ids.isEmpty());

            // display popup
            e->accept();
            int r = actions.value(p.exec(e->globalPos()));

            if (r > 10000)
                actionActivated(r - 10000);

            return true;
        }
    } else if (object == dlg->le_filter && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            dlg->le_filter->clear();
            return true;
        }
    }

    return false;
}

void DiscoDlg::Private::actionActivated(int id)
{
    DiscoListItem *it = DiscoListItem::fromItem(dlg->lv_disco->currentItem());
    if (!it)
        return;

    emit dlg->featureActivated(Features::feature(id), it->item().jid(), it->item().node());
}

void DiscoDlg::Private::objectDestroyed(QObject *obj)
{
    if (obj == busy)
        busy = nullptr;
}

//----------------------------------------------------------------------------
// DiscoDlg
//----------------------------------------------------------------------------

DiscoDlg::DiscoDlg(PsiAccount *pa, const Jid &jid, const QString &node) : QDialog(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint
                   | Qt::CustomizeWindowHint);
    setupUi(this);
    // restore options
    ck_autoItems->setChecked(
        PsiOptions::instance()->getOption("options.ui.service-discovery.automatically-get-items").toBool());
    ck_autoInfo->setChecked(
        PsiOptions::instance()->getOption("options.ui.service-discovery.automatically-get-info").toBool());

    // initialize
    d       = new Private(this, pa);
    d->jid  = jid;
    d->node = node;
    d->pa->dialogRegister(this);
    d->itemsPerPage = PsiOptions::instance()->getOption("options.ui.service-discovery.items-per-page").toInt();

    // setWindowTitle(CAP(caption()));
    setWindowIcon(PsiIconset::instance()->transportStatus("transport", STATUS_ONLINE).icon());
    X11WM_CLASS("disco");

    connect(pb_browse, SIGNAL(clicked()), d, SLOT(doDiscoUserInput()));
    pb_browse->setDefault(false);
    pb_browse->setAutoDefault(false);

    connect(pb_close, SIGNAL(clicked()), this, SLOT(close()));
    pb_close->setDefault(false);
    pb_close->setAutoDefault(false);

    cb_address->addItems(pa->psi()->recentBrowseList()); // FIXME
    cb_address->setFocus();
    cb_address->setEditText(d->jid.full());

    cb_node->addItems(pa->psi()->recentNodeList());
    cb_node->setCurrentIndex(cb_node->findText(node));

    // connectAddressNodeSignals();
    if (pa->loggedIn())
        d->doDiscoUserInput();
}

DiscoDlg::~DiscoDlg()
{
    d->pa->dialogUnregister(this);
    delete d;

    // save options
    PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-items",
                                      bool(ck_autoItems->isChecked()));
    PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-info",
                                      bool(ck_autoInfo->isChecked()));
}

void DiscoDlg::connectAddressNodeSignals()
{
    connect(cb_address->lineEdit(), &QLineEdit::editingFinished, d, [this]() {
        if (cb_address->currentText() != d->addressLastText) {
            d->doDiscoUserInput();
        }
    });
    connect(cb_address, qOverload<int>(&QComboBox::currentIndexChanged), d, [this](int) {
        if (cb_address->currentText() != d->addressLastText) {
            d->doDiscoUserInput();
        }
    });

    connect(cb_node->lineEdit(), &QLineEdit::editingFinished, d, [this]() {
        if (cb_node->currentText() != d->nodeLastText) {
            d->doDiscoUserInput();
        }
    });
    connect(cb_node, qOverload<int>(&QComboBox::currentIndexChanged), d, [this](int) {
        if (cb_node->currentText() != d->nodeLastText) {
            d->doDiscoUserInput();
        }
    });
}

void DiscoDlg::doDisco(QString host, QString node) { d->doDisco(host, node); }

int DiscoDlg::itemsPerPage() const
{
    if (d->itemsPerPage <= 0)
        return 0;
    if (d->itemsPerPage < 10)
        return 10;
    return d->itemsPerPage;
}

PsiAccount *DiscoDlg::account() { return d->pa; }

#include "discodlg.moc"
