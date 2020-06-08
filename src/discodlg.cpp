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

#include "accountlabel.h"
#include "bookmarkmanager.h"
#include "busywidget.h"
#include "common.h"
#include "iconaction.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "stretchwidget.h"
#include "tasklist.h"
#include "xmpp_tasks.h"

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

#define MoreItemsType QTreeWidgetItem::UserType

PsiIcon category2icon(PsiAccount *acc, const Jid &jid, const QString &category, const QString &type,
                      int status = STATUS_ONLINE)
{
    // TODO: update this to http://www.jabber.org/registrar/disco-categories.html#gateway

    // still have to add more options...
    if (category == "service" || category == "gateway") {
        QString trans;

        if (type == "facebook")
            trans = "facebook";
        else if (type == "icq")
            trans = "icq";
        else if (type == "skype")
            trans = "skype";
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
                    auto is = int(qApp->fontMetrics().height() * EqTextIconK + .5);
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

class DiscoListItem;

class DiscoConnector : public QObject {
    Q_OBJECT
public:
    DiscoConnector(QObject *parent) : QObject(parent) { }

signals:
    void itemUpdated(QTreeWidgetItem *);

private:
    friend class DiscoListItem;
};

struct DiscoData {
    PsiAccount *    pa;
    TaskList *      tasks;
    DiscoConnector *d;
};

//----------------------------------------------------------------------------
// DiscoBaseItem
//----------------------------------------------------------------------------
class DiscoBaseItem : public QObject, public QTreeWidgetItem {
public:
    DiscoBaseItem(QTreeWidget *parent, int type = Type);
    DiscoBaseItem(QTreeWidgetItem *parent, int type = Type);
    ~DiscoBaseItem() { }

    virtual const DiscoItem &item() const;
    virtual void             itemSelected() { }
    virtual void             itemClicked() { }

    virtual bool operator<(const QTreeWidgetItem &other) const;
};

DiscoBaseItem::DiscoBaseItem(QTreeWidget *parent, int type) : QTreeWidgetItem(parent, type) { }

DiscoBaseItem::DiscoBaseItem(QTreeWidgetItem *parent, int type) : QTreeWidgetItem(parent, type) { }

const DiscoItem &DiscoBaseItem::item() const
{
    static DiscoItem di;
    return di;
}

bool DiscoBaseItem::operator<(const QTreeWidgetItem &other) const
{
    if (type() == MoreItemsType || other.type() == MoreItemsType) {
        return type() < other.type() && treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder;
    }
    return QTreeWidgetItem::operator<(other);
}

//----------------------------------------------------------------------------
// DiscoExtraItem
//----------------------------------------------------------------------------

class DiscoListItem;
class DiscoExtraItem : public DiscoBaseItem {
public:
    DiscoExtraItem(DiscoListItem *parent, const QString &text);
    virtual ~DiscoExtraItem() { }

    virtual void itemClicked();

private:
    void init(const QString &text);

private:
    DiscoListItem *_parent;
};

//----------------------------------------------------------------------------
// DiscoListItem
//----------------------------------------------------------------------------

class DiscoListItem : public DiscoBaseItem {
    Q_OBJECT
public:
    DiscoListItem(DiscoItem it, DiscoData *d, QTreeWidget *parent);
    DiscoListItem(DiscoItem it, DiscoData *d, QTreeWidgetItem *parent);
    ~DiscoListItem();

    void             setExpanded(bool expand);
    const DiscoItem &item() const;

    void itemSelected();

public slots: // the two are used internally by class, and also called by DiscoDlg::Private::refresh()
    void    updateInfo();
    void    updateItems(bool parentAutoItems = false, bool more = false);
    QString getErrorInfo() const;

private slots:
    void discoItemsFinished();
    void discoInfoFinished();

private:
    DiscoItem            di;
    DiscoData *          d;
    bool                 isRoot;
    bool                 alreadyItems, alreadyInfo;
    bool                 autoItems; // used in updateItemsFinished
    bool                 autoInfo;
    QString              errorInfo;
    DiscoExtraItem *     moreItem;
    SubsetsClientManager subsets;

    void    copyItem(const DiscoItem &);
    void    updateInfo(const DiscoItem &);
    void    updateItemsFinished(const DiscoList &);
    void    autoItemsChildren() const; // automatically call disco#items for children :-)
    QString hash() { return computeHash(item().jid().full(), item().node()); }
    QString computeHash(QString jid, QString node);

    // helper functions
    void init(DiscoItem it, DiscoData *dd);

    void hideChildIndicator();

    bool      autoItemsEnabled() const;
    bool      autoInfoEnabled() const;
    int       itemsPerPage() const;
    DiscoDlg *dlg() const;
};

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, QTreeWidget *parent) : DiscoBaseItem(parent)
{
    isRoot = true;

    init(it, _d);
}

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, QTreeWidgetItem *parent) : DiscoBaseItem(parent)
{
    isRoot = false;

    init(it, _d);
}

DiscoListItem::~DiscoListItem() { delete moreItem; }

void DiscoListItem::init(DiscoItem _item, DiscoData *_d)
{
    d  = _d;
    di = _item;
    copyItem(_item);

    alreadyItems = alreadyInfo = false;

    if (!autoItemsEnabled())
        setChildIndicatorPolicy(ShowIndicator);

    autoInfo = false;
    if (autoInfoEnabled() || isRoot) {
        updateInfo();
        if (!isRoot)
            autoInfo = true;
    }
    moreItem = nullptr;
}

void DiscoListItem::copyItem(const DiscoItem &it)
{
    if (!(!di.jid().full().isEmpty() && it.jid().full().isEmpty()))
        di.setJid(it.jid());
    if (!(!di.node().isEmpty() && it.node().isEmpty()))
        di.setNode(it.node());
    if (!(!di.name().isEmpty() && it.name().isEmpty()))
        di.setName(it.name());

    if (di.name().isEmpty() && !di.jid().full().isEmpty()) // use JID in the Name column
        di.setName(di.jid().full());

    di.setAction(it.action());

    if (!(!di.features().list().isEmpty() && it.features().list().isEmpty()))
        di.setFeatures(it.features());
    if (!(!di.identities().isEmpty() && it.identities().isEmpty()))
        di.setIdentities(it.identities());

    if (di.jid().bare().left(4) == "jud." || di.jid().bare().left(6) == "users.") {
        // nasty hack for the nasty (and outdated) JUD service :-/
        if (!di.features().hasSearch()) {
            QStringList features = di.features().list();
            features << "jabber:iq:search";
            di.setFeatures(features);
        }

        bool                                 found = false;
        DiscoItem::Identities::ConstIterator it    = di.identities().begin();
        for (; it != di.identities().end(); ++it) {
            if ((*it).category == "service" && (*it).type == "jud") {
                found = true;
                break;
            }
        }
        if (!found) {
            DiscoItem::Identity id;
            id.category = "service";
            id.type     = "jud";
            DiscoItem::Identities ids;
            ids << id;
            di.setIdentities(ids);
        }
    }

    setText(0, di.name().simplified());
    setText(1, di.jid().full());
    setText(2, di.node().simplified());

    bool iconOk = false;
    if (!di.identities().isEmpty()) {
        DiscoItem::Identity id = di.identities().first();

        if (!id.category.isEmpty()) {
            QIcon icon = category2icon(d->pa, di.jid(), id.category, id.type).icon();

            if (!icon.isNull()) {
                setIcon(0, icon);
                iconOk = true;
            }
        }
    }

    if (!iconOk)
        setIcon(0, PsiIconset::instance()->status(di.jid(), STATUS_ONLINE).icon());

    if (isSelected()) // update actions
        emit d->d->itemUpdated(this);
}

QString DiscoListItem::getErrorInfo() const { return errorInfo; }

const DiscoItem &DiscoListItem::item() const { return di; }

DiscoDlg *DiscoListItem::dlg() const { return static_cast<DiscoDlg *>(treeWidget()->parent()); }

bool DiscoListItem::autoItemsEnabled() const { return dlg()->ck_autoItems->isChecked(); }

bool DiscoListItem::autoInfoEnabled() const { return dlg()->ck_autoInfo->isChecked(); }

int DiscoListItem::itemsPerPage() const { return dlg()->itemsPerPage(); }

void DiscoListItem::setExpanded(bool expand)
{
    if (expand) {
        if (!alreadyItems)
            updateItems();
        else
            autoItemsChildren();
    }

    QTreeWidgetItem::setExpanded(expand);
}

void DiscoListItem::itemSelected()
{
    if (!alreadyInfo)
        updateInfo();
}

void DiscoListItem::updateItems(bool parentAutoItems, bool more)
{
    if (parentAutoItems) {
        // save traffic
        if (alreadyItems)
            return;

        // FIXME: currently, JUD doesn't seem to answer to browsing requests
        if (item().identities().size()) {
            DiscoItem::Identity id = item().identities().first();
            if (id.category == "service" && id.type == "jud")
                return;
        }
        QString j = item().jid().domain(); // just another method to discover if we're gonna to browse JUD
        if (item().jid().node().isEmpty() && (j.left(4) == "jud." || j.left(6) == "users."))
            return;
    }

    autoItems = !parentAutoItems;

    if (!autoItemsEnabled())
        autoItems = false;

    JT_DiscoItems *jt = new JT_DiscoItems(d->pa->client()->rootTask());
    connect(jt, SIGNAL(finished()), SLOT(discoItemsFinished()));
    int max = itemsPerPage();
    if (max != 0) {
        subsets.setMax(max);
        if (!more)
            subsets.getFirst();
        else
            subsets.getNext();
        jt->includeSubsetQuery(subsets);
    }
    jt->get(di.jid(), di.node());
    jt->go(true);
    d->tasks->append(jt);
}

void DiscoListItem::discoItemsFinished()
{
    JT_DiscoItems *jt = static_cast<JT_DiscoItems *>(sender());

    if (jt->success()) {
        jt->extractSubsetInfo(subsets);
        updateItemsFinished(jt->items());
    } else if (!autoItems) {
        QString error = jt->statusString();
        QMessageBox::critical(dlg(), tr("Error"),
                              tr("There was an error getting items for <b>%1</b>.<br>Reason: %2")
                                  .arg(di.jid().full())
                                  .arg(QString(error).replace('\n', "<br>")));
    }

    alreadyItems = true;
}

QString DiscoListItem::computeHash(QString jid, QString node)
{
    QString ret = jid.replace('@', "\\@");
    ret += "@";
    ret += node.replace('@', "\\@");
    return ret;
}

void DiscoListItem::hideChildIndicator()
{
    setChildIndicatorPolicy(DontShowIndicator);
    emitDataChanged();
}

void DiscoListItem::updateItemsFinished(const DiscoList &list)
{
    treeWidget()->setUpdatesEnabled(false);

    QHash<QString, DiscoListItem *> children;
    DiscoBaseItem *                 child = static_cast<DiscoBaseItem *>(QTreeWidgetItem::child(0));
    DiscoListItem *                 item;
    for (int i = 1; child; ++i) {
        if (!moreItem && child->type() != MoreItemsType) {
            item = static_cast<DiscoListItem *>(child);
            children.insert(item->hash(), item);
        }
        child = static_cast<DiscoListItem *>(QTreeWidgetItem::child(i));
    }

    // add/update items
    for (DiscoList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        const DiscoItem a = *it;

        QString key = computeHash(a.jid().full(), a.node());
        item        = children[key];

        if (item) {
            item->copyItem(a);
            children.remove(key);
        } else {
            new DiscoListItem(a, d, this);
        }
    }

    // remove all items that are not on new DiscoList
    qDeleteAll(children);
    children.clear();

    // create moreItem if it needs
    if (subsets.isValid() && !subsets.isLast()) {
        if (!moreItem && childCount() != 0)
            moreItem = new DiscoExtraItem(this, tr("more items", "Getting more disco items in order page by page"));
    } else if (moreItem) {
        delete moreItem;
        moreItem = nullptr;
    }

    if (autoItems && isExpanded())
        autoItemsChildren();

    if (childCount() == 0) {
        hideChildIndicator();
    }

    // root item is initially hidden
    if (isRoot && isHidden())
        setHidden(false);

    treeWidget()->setUpdatesEnabled(true);
}

void DiscoListItem::autoItemsChildren() const
{
    if (!autoItemsEnabled())
        return;

    DiscoListItem *child = static_cast<DiscoListItem *>(QTreeWidgetItem::child(0));
    for (int i = 1; child; ++i) {
        child->updateItems(true);

        child = static_cast<DiscoListItem *>(QTreeWidgetItem::child(i));
    }
}

void DiscoListItem::updateInfo()
{
    JT_DiscoInfo *jt = new JT_DiscoInfo(d->pa->client()->rootTask());
    jt->setAllowCache(
        false); // Workaround for a bug https://github.com/hanzz/spectrum2/issues/205 (invalid caps from transport)
    connect(jt, SIGNAL(finished()), SLOT(discoInfoFinished()));
    jt->get(di.jid(), di.node());
    jt->go(true);
    d->tasks->append(jt);
}

void DiscoListItem::discoInfoFinished()
{
    JT_DiscoInfo *jt = static_cast<JT_DiscoInfo *>(sender());

    if (jt->success()) {
        updateInfo(jt->item());
    } else {
        QString error_str  = jt->statusString();
        int     error_code = jt->statusCode();
        hideChildIndicator();

        // we change the icon for the items with disco#info returning type=="cancel" || type=="wait" error codes
        // based on XEP-0086

        // FIXME: use another method for checking XMPP error-types when Iris will provide one
        if (error_code == 400 || error_code == 404 || error_code == 405 || error_code == 409 || error_code == 500
            || error_code == 501 || error_code == 503 || error_code == 504) {
            bool iconOk = false;
            if (!di.identities().isEmpty()) {
                DiscoItem::Identity id = di.identities().first();
                if (!id.category.isEmpty()) {
                    QIcon icon = category2icon(d->pa, di.jid(), id.category, id.type, STATUS_ERROR).icon();

                    if (!icon.isNull()) {
                        setIcon(0, icon);
                        iconOk = true;
                    }
                }
            }
            if (!iconOk)
                setIcon(0, PsiIconset::instance()->status(di.jid(), STATUS_ERROR).icon());
        }

        errorInfo = QString("%1").arg(QString(error_str).replace('\n', "<br>"));

        if (!autoInfo) {
            QMessageBox::critical(dlg(), tr("Error"),
                                  tr("There was an error getting item's info for <b>%1</b>.<br>Reason: %2")
                                      .arg(di.jid().full())
                                      .arg(QString(error_str).replace('\n', "<br>")));
        }
    }

    alreadyInfo = true;
    autoInfo    = false;
}

void DiscoListItem::updateInfo(const DiscoItem &item)
{
    copyItem(item);

    if (isRoot && isHidden())
        setHidden(false);
}

//----------------------------------------------------------------------------
// DiscoExtraItem implementation
//----------------------------------------------------------------------------

DiscoExtraItem::DiscoExtraItem(DiscoListItem *parent, const QString &text) : DiscoBaseItem(parent, MoreItemsType)
{
    _parent = parent;
    init(text);
}

void DiscoExtraItem::itemClicked() { _parent->updateItems(false, true); }

void DiscoExtraItem::init(const QString &text)
{
    Qt::ItemFlags l = flags() & ~Qt::ItemIsSelectable;
    l |= Qt::ItemNeverHasChildren;
    setFlags(l);
    setText(0, text);
    QFont f = font(0);
    f.setItalic(true);
    setFont(0, f);
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

    // NAME <JID> (Node "NODE")
    //
    // Identities:
    // (icon) NAME (Category "CATEGORY"; Type "TYPE")
    // (icon) NAME (Category "CATEGORY"; Type "TYPE")
    //
    // Features:
    // NAME (http://jabber.org/feature)
    // NAME (http://jabber.org/feature)

    // top row
    QString   text = "<qt><nobr>";
    DiscoItem item = i->item();

    if (item.name() != item.jid().full())
        text += item.name() + " ";

    text += "&lt;" + item.jid().full() + "&gt;";

    if (!item.node().isEmpty())
        text += " (" + tr("Node") + " \"" + item.node() + "\")";

    text += "</nobr>";

    if (!item.identities().isEmpty() || !item.features().list().isEmpty())
        text += "<br>\n";

    // identities
    if (!item.identities().isEmpty()) {
        text += "<br>\n<b>" + tr("Identities:") + "</b>\n";

        DiscoItem::Identities::ConstIterator it = item.identities().begin();
        for (; it != item.identities().end(); ++it) {
            text += "<br>";
            PsiIcon icon(category2icon(dlg->account(), item.jid(), (*it).category, (*it).type));
            if (!icon.name().isEmpty())
                text += "<icon name=\"" + icon.name() + "\"> ";
            text += (*it).name;
            text += " (" + tr("Category") + " \"" + (*it).category + "\"; " + tr("Type") + " \"" + (*it).type + "\")\n";
        }

        if (!item.features().list().isEmpty())
            text += "<br>\n";
    }

    // features
    if (!item.features().list().isEmpty()) {
        text += "<br>\n<b>" + tr("Features:") + "</b>\n";

        QStringList                features = item.features().list();
        QStringList::ConstIterator it       = features.begin();
        for (; it != features.end(); ++it) {
            Features f(*it);
            text += "\n<br>";
            if (f.id() > Features::FID_None)
                text += f.name() + " (";
            text += *it;
            if (f.id() > Features::FID_None)
                text += ")";
        }
    }

    QString errorInfo = i->getErrorInfo();
    if (!errorInfo.isEmpty()) {
        text += "<br>\n<br>\n<b>" + tr("Error:") + "</b>\n";
        text += errorInfo;
    }

    text += "</qt>";
    //    QRect r( visualItemRect(i) );
    PsiToolTip::showText(pos, text, this);
    return true;
}

bool DiscoListView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::ToolTip && o->isWidgetType()) {
        QWidget *   w  = static_cast<QWidget *>(o);
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

    DiscoData data;

    QToolBar *  toolBar;
    IconAction *actBrowse, *actBack, *actForward, *actRefresh, *actStop;

    // custom actions, that will be added to toolbar and context menu
    IconAction *actRegister, *actUnregister, *actSearch, *actJoin, *actAHCommand, *actVCard, *actAdd, *actQueryVersion;

    typedef QList<History *> HistoryList;
    HistoryList              backHistory, forwardHistory;

    BusyWidget *busy;

    int itemsPerPage;

public: // functions
    Private(DiscoDlg *parent, PsiAccount *pa);
    ~Private();

public slots:
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
    dlg        = parent;
    data.pa    = pa;
    data.tasks = new TaskList;
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
    delete data.tasks;
}

void DiscoDlg::Private::doDisco(QString _host, QString _node, bool doHistory)
{
    PsiAccount *pa = data.pa;
    if (!pa->checkConnected(dlg))
        return;

    // Strip whitespace
    Jid     j;
    QString host = _host;
    if (host.isEmpty())
        host = dlg->cb_address->currentText();
    j = host.trimmed();
    if (!j.isValid())
        return;

    QString n = _node.trimmed();
    if (n.isEmpty())
        n = dlg->cb_node->currentText().trimmed();

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

    data.tasks->clear(); // also will call all all necessary functions
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
    data.pa->psi()->recentBrowseAdd(j.full());
    dlg->cb_address->clear();
    dlg->cb_address->addItems(data.pa->psi()->recentBrowseList());

    data.pa->psi()->recentNodeAdd(n);
    dlg->cb_node->clear();
    dlg->cb_node->addItems(data.pa->psi()->recentNodeList());
}

void DiscoDlg::Private::actionStop() { data.tasks->clear(); }

void DiscoDlg::Private::actionRefresh()
{
    DiscoListItem *it = static_cast<DiscoListItem *>(dlg->lv_disco->currentItem());
    if (!it)
        return;

    it->updateItems();
    it->updateInfo();
}

void DiscoDlg::Private::actionBrowse()
{
    DiscoListItem *it = static_cast<DiscoListItem *>(dlg->lv_disco->currentItem());
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
    DiscoListItem *it = static_cast<DiscoListItem *>(item);
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

    DiscoListItem * it = static_cast<DiscoListItem *>(item);
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
            QMenu *               fm = new QMenu(&p);
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
    DiscoListItem *it = static_cast<DiscoListItem *>(dlg->lv_disco->currentItem());
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
    d->data.pa->dialogRegister(this);
    d->itemsPerPage = PsiOptions::instance()->getOption("options.ui.service-discovery.items-per-page").toInt();

    // setWindowTitle(CAP(caption()));
    setWindowIcon(PsiIconset::instance()->transportStatus("transport", STATUS_ONLINE).icon());
    X11WM_CLASS("disco");

    connect(pb_browse, SIGNAL(clicked()), d, SLOT(doDisco()));
    pb_browse->setDefault(false);
    pb_browse->setAutoDefault(false);

    connect(pb_close, SIGNAL(clicked()), this, SLOT(close()));
    pb_close->setDefault(false);
    pb_close->setAutoDefault(false);

    cb_address->addItems(pa->psi()->recentBrowseList()); // FIXME
    cb_address->setFocus();
    connect(cb_address, SIGNAL(activated(const QString &)), d, SLOT(doDisco()));
    cb_address->setEditText(d->jid.full());

    cb_node->addItems(pa->psi()->recentNodeList());
    connect(cb_node, SIGNAL(activated(const QString &)), d, SLOT(doDisco()));
    cb_node->setCurrentIndex(cb_node->findText(node));

    if (pa->loggedIn())
        doDisco();
}

DiscoDlg::~DiscoDlg()
{
    d->data.pa->dialogUnregister(this);
    delete d;

    // save options
    PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-items",
                                      bool(ck_autoItems->isChecked()));
    PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-info",
                                      bool(ck_autoInfo->isChecked()));
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

PsiAccount *DiscoDlg::account() { return d->data.pa; }

#include "discodlg.moc"
