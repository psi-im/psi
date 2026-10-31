/*
 * discoitems.cpp - Service Discovery tree items
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

#include "discoitems_p.h"
#include "tasklist.h"
#include <QCoreApplication>
#include <QHash>
#include <QHeaderView>
#include <algorithm>
#include <utility>

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
        if (type() == other.type())
            return false;
        const bool ascending = !treeWidget() || treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder;
        return type() == MoreItemsType ? !ascending : ascending;
    }
    return QTreeWidgetItem::operator<(other);
}

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, QTreeWidget *parent) : DiscoBaseItem(parent), isRoot(true)
{
    init(it, _d);
}

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, QTreeWidgetItem *parent, bool isDeifinitelyLeaf) :
    DiscoBaseItem(parent), isRoot(false), isDefinitelyLeaf(isDeifinitelyLeaf)
{
    init(it, _d);
}

static void cancelRequest(Task *task, QObject *receiver)
{
    if (task) {
        QObject::disconnect(task, nullptr, receiver, nullptr);
        task->safeDelete();
    }
}

void DiscoData::cancelTasks()
{
    for (auto *task : std::as_const(*tasks)) {
        QObject::disconnect(task, SIGNAL(finished()), nullptr, nullptr);
        task->safeDelete();
    }
}

DiscoListItem *DiscoListItem::fromItem(QTreeWidgetItem *item)
{
    return item && item->type() != MoreItemsType ? static_cast<DiscoListItem *>(item) : nullptr;
}

DiscoListItem::~DiscoListItem()
{
    cancelRequest(itemsTask, this);
    cancelRequest(infoTask, this);
    delete moreItem;
}

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
        updateInfo(!isRoot);
    }
    if (isDefinitelyLeaf) {
        hideChildIndicator();
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

        bool        found  = false;
        const auto &idents = di.identities();
        for (const auto &id : idents) {
            if (id.category == "service" && id.type == "jud") {
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

    setIcon(0, d->itemIcon(di, false));

    if (isSelected()) // update actions
        emit d->d->itemUpdated(this);
}

QString DiscoListItem::getErrorInfo() const { return errorInfo; }

const DiscoItem &DiscoListItem::item() const { return di; }

bool DiscoListItem::autoItemsEnabled() const { return d->autoItemsEnabled(); }

bool DiscoListItem::autoInfoEnabled() const { return d->autoInfoEnabled(); }

int DiscoListItem::itemsPerPage() const { return d->itemsPerPage(); }

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
    if (isDefinitelyLeaf || !d->rootTask)
        return;
    if (more && (itemsTask || !hasMoreItems))
        return;
    if (parentAutoItems) {
        // save traffic
        if (alreadyItems || itemsTask)
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

    cancelRequest(itemsTask, this);
    appendItems      = more;
    interactiveItems = !parentAutoItems;
    autoItems        = !parentAutoItems;

    if (!autoItemsEnabled())
        autoItems = false;

    JT_DiscoItems *jt = new JT_DiscoItems(d->rootTask);
    itemsTask         = jt;
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
    d->tasks->append(jt);
    jt->go(true);
}

void DiscoListItem::discoItemsFinished()
{
    auto *jt = qobject_cast<JT_DiscoItems *>(sender());
    if (!jt || jt != itemsTask)
        return;
    itemsTask    = nullptr;
    alreadyItems = jt->success();
    if (jt->success()) {
        hasMoreItems = jt->extractSubsetInfo(subsets) && subsets.isValid() && !subsets.isLast();
        updateItemsFinished(jt->items());
    } else if (interactiveItems) {
        // The error dialog can run a nested event loop and destroy this item.
        const auto showError = d->showError;
        showError(tr("There was an error getting items for <b>%1</b>.<br>Reason: %2")
                      .arg(di.jid().full().toHtmlEscaped(), jt->statusString().toHtmlEscaped().replace('\n', "<br>")));
    }
}

void DiscoListItem::hideChildIndicator()
{
    setChildIndicatorPolicy(DontShowIndicator);
    emitDataChanged();
}

void DiscoListItem::updateItemsFinished(const DiscoList &list)
{
    treeWidget()->setUpdatesEnabled(false);

    struct Child {
        DiscoListItem *item;
        bool           seen = false;
    };
    QHash<QPair<QString, QString>, Child> children;
    QTreeWidgetItem                      *child = QTreeWidgetItem::child(0);
    for (int i = 1; child; ++i) {
        if (auto *item = fromItem(child))
            children.insert(qMakePair(item->item().jid().full(), item->item().node()), { item, false });
        child = QTreeWidgetItem::child(i);
    }

    const auto &identities = di.identities();
    const bool  canPubsub  = std::any_of(identities.begin(), identities.end(),
                                         [](const auto &id) { return id.category == QLatin1String("pubsub"); });
    for (const auto &a : list) {
        const auto key = qMakePair(a.jid().full(), a.node());
        auto       it  = children.find(key);
        if (it != children.end()) {
            it->item->copyItem(a);
            it->seen = true;
        } else {
            auto *item = new DiscoListItem(a, d, this, !isRoot && canPubsub && a.node().isEmpty());
            children.insert(key, { item, true });
        }
    }

    // A continuation adds to the current result; a refresh replaces it.
    if (!appendItems) {
        for (const auto &entry : std::as_const(children)) {
            if (!entry.seen)
                delete entry.item;
        }
    }

    // create moreItem if it needs
    if (hasMoreItems) {
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

    QTreeWidgetItem *child = QTreeWidgetItem::child(0);
    for (int i = 1; child; ++i) {
        if (auto *item = fromItem(child))
            item->updateItems(true);
        child = QTreeWidgetItem::child(i);
    }
}

void DiscoListItem::updateInfo(bool automatic)
{
    if (!automatic)
        autoInfo = false;
    if (infoTask)
        return;
    autoInfo = automatic;
    if (isDefinitelyLeaf || !d->rootTask) {
        return;
    }
    JT_DiscoInfo *jt = new JT_DiscoInfo(d->rootTask);
    infoTask         = jt;
    jt->setAllowCache(
        false); // Workaround for a bug https://github.com/hanzz/spectrum2/issues/205 (invalid caps from transport)
    connect(jt, SIGNAL(finished()), SLOT(discoInfoFinished()));
    jt->get(di.jid(), di.node());
    d->tasks->append(jt);
    jt->go(true);
}

void DiscoListItem::discoInfoFinished()
{
    auto *jt = qobject_cast<JT_DiscoInfo *>(sender());
    if (!jt || jt != infoTask)
        return;
    infoTask               = nullptr;
    alreadyInfo            = jt->success();
    const bool interactive = !autoInfo;
    autoInfo               = false;

    if (jt->success()) {
        errorInfo.clear();
        updateInfo(jt->item());
    } else {
        QString error_str  = jt->statusString();
        int     error_code = jt->statusCode();
        hideChildIndicator();

        // we change the icon for the items with disco#info returning type=="cancel" || type=="wait" error codes
        // based on XEP-0086

        // FIXME: use another method for checking XMPP error-types when Iris will provide one
        if (error_code == 400 || error_code == 404 || error_code == 405 || error_code == 409 || error_code == 500
            || error_code == 501 || error_code == 503 || error_code == 504)
            setIcon(0, d->itemIcon(di, true));

        errorInfo = error_str;

        if (interactive) {
            const auto showError = d->showError;
            showError(tr("There was an error getting item's info for <b>%1</b>.<br>Reason: %2")
                          .arg(di.jid().full().toHtmlEscaped(), error_str.toHtmlEscaped().replace('\n', "<br>")));
        }
    }
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

QString discoItemToolTip(const DiscoItem &item, const QString &errorInfo, const QStringList &identityIcons)
{
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
    QString text = "<qt><nobr>";

    if (item.name() != item.jid().full())
        text += item.name().toHtmlEscaped() + " ";

    text += "&lt;" + item.jid().full().toHtmlEscaped() + "&gt;";

    if (!item.node().isEmpty())
        text += " (" + QCoreApplication::translate("DiscoListView", "Node") + " \"" + item.node().toHtmlEscaped()
            + "\")";

    text += "</nobr>";

    if (!item.identities().isEmpty() || !item.features().list().isEmpty())
        text += "<br>\n";

    // identities
    if (!item.identities().isEmpty()) {
        text += "<br>\n<b>" + QCoreApplication::translate("DiscoListView", "Identities:") + "</b>\n";

        int         identityIndex = 0;
        const auto &idents        = item.identities();
        for (const auto &id : idents) {
            text += "<br>";
            const auto icon = identityIcons.value(identityIndex++);
            if (!icon.isEmpty())
                text += "<icon name=\"" + icon.toHtmlEscaped() + "\"> ";
            text += id.name.toHtmlEscaped();
            text += " (" + QCoreApplication::translate("DiscoListView", "Category") + " \""
                + id.category.toHtmlEscaped() + "\"; " + QCoreApplication::translate("DiscoListView", "Type") + " \""
                + id.type.toHtmlEscaped() + "\")\n";
        }

        if (!item.features().list().isEmpty())
            text += "<br>\n";
    }

    // features
    if (!item.features().list().isEmpty()) {
        text += "<br>\n<b>" + QCoreApplication::translate("DiscoListView", "Features:") + "</b>\n";

        const QStringList features = item.features().list();
        for (const auto &feature : features) {
            Features f(feature);
            text += "\n<br>";
            if (f.id() > Features::FID_None)
                text += f.name().toHtmlEscaped() + " (";
            text += feature.toHtmlEscaped();
            if (f.id() > Features::FID_None)
                text += ")";
        }
    }

    if (!errorInfo.isEmpty()) {
        text += "<br>\n<br>\n<b>" + QCoreApplication::translate("DiscoListView", "Error:") + "</b>\n";
        text += errorInfo.toHtmlEscaped().replace('\n', "<br>");
    }

    text += "</qt>";
    return text;
}
