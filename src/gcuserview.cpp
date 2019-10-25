/*
 * gcuserview.cpp - groupchat roster
 * Copyright (C) 2001-2002  Justin Karneges
 * Copyright (C) 2011  Evgeny Khryukin
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

#include "gcuserview.h"

#include "avatars.h"
#include "coloropt.h"
#include "common.h"
#include "groupchatdlg.h"
#include "psiaccount.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "userlist.h"
#include "xmpp_caps.h"
#include "xmpp_muc.h"

#include <QItemDelegate>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

// static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
//{
//    return s1.toLower() < s2.toLower();
//}

//----------------------------------------------------------------------------
// GCUserViewDelegate
//----------------------------------------------------------------------------
class GCUserViewDelegate : public QItemDelegate {
    Q_OBJECT
public:
    GCUserViewDelegate(QObject *p) : QItemDelegate(p) { updateSettings(); }

    void updateSettings()
    {
        PsiOptions *o    = PsiOptions::instance();
        colorForeground_ = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-foreground");
        colorBackground_ = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-background");
        colorModerator_  = o->getOption("options.ui.look.colors.muc.role-moderator").value<QColor>();
        colorParticipant_ = o->getOption("options.ui.look.colors.muc.role-participant").value<QColor>();
        colorVisitor_     = o->getOption("options.ui.look.colors.muc.role-visitor").value<QColor>();
        colorNoRole_      = o->getOption("options.ui.look.colors.muc.role-norole").value<QColor>();
        showGroups_       = o->getOption("options.ui.muc.userlist.show-groups").toBool();
        slimGroups_       = o->getOption("options.ui.muc.userlist.use-slim-group-headings").toBool();
        nickColoring_     = o->getOption("options.ui.muc.userlist.nick-coloring").toBool();
        showClients_      = o->getOption("options.ui.muc.userlist.show-client-icons").toBool();
        showAffiliations_ = o->getOption("options.ui.muc.userlist.show-affiliation-icons").toBool();
        showStatusIcons_  = o->getOption("options.ui.muc.userlist.show-status-icons").toBool();
        showAvatar_       = o->getOption("options.ui.muc.userlist.avatars.show").toBool();
        avatarSize_       = pointToPixel(o->getOption("options.ui.muc.userlist.avatars.size").toInt());
        avatarAtLeft_     = o->getOption("options.ui.muc.userlist.avatars.avatars-at-left").toBool();
        avatarRadius_     = pointToPixel(o->getOption("options.ui.muc.userlist.avatars.radius").toInt());

        QFont font;
        font.fromString(o->getOption("options.ui.look.font.contactlist").toString());
        fontHeight_ = QFontMetrics(font).height() + 2;
    }

    void paint(QPainter *mp, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (index.parent().isValid()) {
            paintContact(mp, option, index);
        } else {
            paintGroup(mp, option, index);
        }
    }

    void paintGroup(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &index) const
    {
        if (!showGroups_)
            return;

        QRect rect = o.rect;
        QFont f    = o.font;
        p->setFont(f);
        if (!slimGroups_ || (o.state & QStyle::State_Selected)) {
            p->fillRect(rect, colorBackground_);
        }

        p->setPen(QPen(colorForeground_));
        rect.translate(2, (rect.height() - o.fontMetrics.height()) / 2);

        QString groupName = index.data().toString();
        int     c         = index.model()->rowCount(index);
        if (c) {
            groupName += QString("  (%1)").arg(c);
        }

        p->drawText(rect, groupName);
        if (slimGroups_ && !(o.state & QStyle::State_Selected)) {
            QFontMetrics fm(f);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            int x = fm.horizontalAdvance(groupName) + 8;
#else
            int x = fm.width(groupName) + 8;
#endif
            int width = rect.width();
            if (x < width - 8) {
                int h = rect.y() + (rect.height() / 2) - 1;
                p->setPen(QPen(colorBackground_));
                p->drawLine(x, h, width - 8, h);
                h++;
                p->setPen(QPen(colorForeground_));
                p->drawLine(x, h, width - 8, h);
            }
        }
    }

    void paintContact(QPainter *mp, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        mp->save();
        QStyleOptionViewItem o       = option;
        QPalette             palette = o.palette;
        MUCItem::Role        r       = index.data(GCUserModel::StatusRole).value<Status>().mucItem().role();
        QRect                rect    = o.rect;

        if (nickColoring_) {
            if (r == MUCItem::Moderator)
                palette.setColor(QPalette::Text, colorModerator_);
            else if (r == MUCItem::Participant)
                palette.setColor(QPalette::Text, colorParticipant_);
            else if (r == MUCItem::Visitor)
                palette.setColor(QPalette::Text, colorVisitor_);
            else
                palette.setColor(QPalette::Text, colorNoRole_);
        }

        mp->fillRect(rect,
                     (o.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight)
                                                        : palette.color(QPalette::Base));

        if (showAvatar_) {
            QPixmap ava = index.data(GCUserModel::AvatarRole).value<QPixmap>();
            if (ava.isNull()) {
                ava = IconsetFactory::iconPixmap("psi/default_avatar");
            }
            ava = AvatarFactory::roundedAvatar(ava, avatarRadius_, avatarSize_);
            QRect avaRect(rect);
            avaRect.setWidth(ava.width());
            avaRect.setHeight(ava.height());
            if (!avatarAtLeft_) {
                avaRect.moveTopRight(rect.topRight());
                avaRect.translate(-1, 1);
                rect.setRight(avaRect.left() - 1);
            } else {
                avaRect.translate(1, 1);
                rect.setLeft(avaRect.right() + 1);
            }
            mp->drawPixmap(avaRect, ava);
        }

        QPixmap status = showStatusIcons_
            ? PsiIconset::instance()->status(index.data(GCUserModel::StatusRole).value<Status>()).pixmap()
            : QPixmap();
        int h  = rect.height();
        int sh = status.isNull() ? 0 : status.height();
        rect.setHeight(qMax(sh, fontHeight_));
        rect.moveTop(rect.top() + (h - rect.height()) / 2);
        if (!status.isNull()) {
            QRect statusRect(rect);
            statusRect.setWidth(status.width());
            statusRect.setHeight(status.height());
            statusRect.translate(1, 1);
            mp->drawPixmap(statusRect, status);
            rect.setLeft(statusRect.right() + 2);
        } else
            rect.setLeft(rect.left() + 2);

        mp->setPen(QPen((o.state & QStyle::State_Selected) ? palette.color(QPalette::HighlightedText)
                                                           : palette.color(QPalette::Text)));
        mp->setFont(o.font);
        mp->setClipRect(rect);
        QTextOption to;
        to.setWrapMode(QTextOption::NoWrap);
        mp->drawText(rect, index.data(Qt::DisplayRole).toString(), to);

        QList<QPixmap> rightPixs;

        if (showAffiliations_) {
            QPixmap pix = index.data(GCUserModel::AffilationIconRole).value<QPixmap>();
            if (!pix.isNull())
                rightPixs.push_back(pix);
        }

        if (showClients_) {
            QPixmap clientPix = index.data(GCUserModel::ClientIconRole).value<QPixmap>();
            if (!clientPix.isNull())
                rightPixs.push_back(clientPix);
        }

        mp->restore();

        if (rightPixs.isEmpty())
            return;

        int sumWidth = 0;
        foreach (const QPixmap &pix, rightPixs) {
            sumWidth += pix.width();
        }
        sumWidth += rightPixs.count();

        QColor bgc = (option.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight)
                                                             : palette.color(QPalette::Base);
        QColor tbgc = bgc;
        tbgc.setAlpha(0);
        QLinearGradient grad(rect.right() - sumWidth - 20, 0, rect.right() - sumWidth, 0);
        grad.setColorAt(0, tbgc);
        grad.setColorAt(1, bgc);
        QBrush tbakBr(grad);
        QRect  gradRect(rect);
        gradRect.setLeft(gradRect.right() - sumWidth - 20);
        mp->fillRect(gradRect, tbakBr);

        QRect iconRect(rect);
        for (int i = 0; i < rightPixs.size(); i++) {
            const QPixmap pix = rightPixs[i];
            iconRect.setRight(iconRect.right() - pix.width() - 1);
            mp->drawPixmap(iconRect.topRight(), pix);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid())
            return QSize(0, 0);

        QSize size = QItemDelegate::sizeHint(option, index);
        if (index.parent().isValid()) {
            QPixmap statusIcon
                = PsiIconset::instance()->status(index.data(GCUserModel::StatusRole).value<Status>()).pixmap();
            int rowH = qMax(fontHeight_, statusIcon.height() + 2);
            int h    = showAvatar_ ? qMax(avatarSize_ + 2, rowH) : rowH;
            size.setHeight(h);
        } else {
            size.setHeight(showGroups_ ? fontHeight_ : 0);
        }

        return size;
    }

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index)
    {
        Q_UNUSED(option);
        QString toolTip = index.data(Qt::ToolTipRole).toString();
        if (toolTip.size()) {
            PsiToolTip::showText(event->globalPos(), toolTip, view);
            return true;
        }
        return false;
    }

private:
    QColor colorForeground_, colorBackground_, colorModerator_, colorParticipant_, colorVisitor_, colorNoRole_;
    bool   showGroups_, slimGroups_, nickColoring_, showClients_, showAffiliations_, showStatusIcons_, showAvatar_,
        avatarAtLeft_;
    int avatarSize_, fontHeight_, avatarRadius_;
};

//----------------------------------------------------------------------------
// GCUserModel
//----------------------------------------------------------------------------

GCUserModel::GCUserModel(PsiAccount *account, const Jid selfJid, QObject *parent) :
    QAbstractItemModel(parent), _account(account), _selfJid(selfJid), _selfContact(nullptr)
{
}

QModelIndex GCUserModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!column) {
        if (parent.isValid()) { // contact
            if (parent.row() < LastGroupRole && row < contacts[parent.row()].size()) {
                // qDebug("Create contact index row=%d column=%d, data=%p", row, column,
                // contacts[parent.row()][row].data());
                return createIndex(row, column, contacts[parent.row()][row].data());
            }
        } else {
            if (row < LastGroupRole) {
                return createIndex(row, column);
            }
        }
    }
    return QModelIndex();
}

QVariant GCUserModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() > 0)
        return QVariant();

    if (index.parent().isValid()) {
        // contact

        Role        groupRole = Role(index.parent().row());
        const auto &cs        = contacts[groupRole];
        if (index.row() >= cs.size()) {
            return QVariant();
        }
        const MUCContact &contact = *(cs.at(index.row()));

        switch (role) {
        case Qt::DisplayRole:
            return contact.name;
        case Qt::ToolTipRole:
            return makeToolTip(contact);
        case StatusRole:
            return QVariant::fromValue<Status>(contact.status);
        case AvatarRole:
            return contact.avatar;
        case ClientIconRole: {
            UserListItem u;
            Jid          jid = _selfJid.withResource(contact.name);
            Jid          caps_jid(
                /*s.mucItem().jid().isEmpty() ? */ jid /* : s.mucItem().jid()*/); // TODO review caching of such caps
            CapsManager *cm             = _account->client()->capsManager();
            QString      client_name    = cm->clientName(caps_jid);
            QString      client_version = (client_name.isEmpty() ? QString() : cm->clientVersion(caps_jid));
            UserResource ur;
            ur.setStatus(contact.status);
            ur.setClient(client_name, client_version, "");
            u.userResourceList().append(ur);
            QStringList clients = u.clients();
            if (!clients.isEmpty())
                return IconsetFactory::iconPixmap("clients/" + clients.takeFirst());
            break;
        }
        case AffilationIconRole: {
            MUCItem::Affiliation a = contact.status.mucItem().affiliation();

            if (a == MUCItem::Owner)
                return IconsetFactory::iconPixmap("affiliation/owner");
            else if (a == MUCItem::Admin)
                return IconsetFactory::iconPixmap("affiliation/admin");
            else if (a == MUCItem::Member)
                return IconsetFactory::iconPixmap("affiliation/member");
            else if (a == MUCItem::Outcast)
                return IconsetFactory::iconPixmap("affiliation/outcast");
            else
                return IconsetFactory::iconPixmap("affiliation/noaffiliation");
        }
        }
    } else {
        // group
        // next list MUST match index to index with GCUserModel::Role
        static QStringList groupNames = QStringList() << tr("Moderators") << tr("Participants") << tr("Visitors");
        switch (role) {
        case Qt::DisplayRole:
            if (index.row() < groupNames.size()) {
                return groupNames[index.row()];
            }
            break;
        }
    }

    return QVariant();
}

GCUserModel::MUCContact *GCUserModel::selfContact() const { return _selfContact.data(); }

void GCUserModel::updateAvatar(const QString &nick)
{
    QModelIndex index = findIndex(nick);
    if (index.isValid()) {
        contacts[index.parent().row()][index.row()]->avatar
            = _account->avatarFactory()->getMucAvatar(_selfJid.withResource(nick));
        emit dataChanged(index, index);
    }
}

QString GCUserModel::makeToolTip(const MUCContact &contact) const
{
    const QString &nick = contact.name;
    UserListItem   u;

    Jid contactJid = _selfJid.withResource(nick);
    u.setJid(contactJid);
    u.setName(nick);

    // Find out capabilities info
    Jid          caps_jid(contactJid);
    CapsManager *cm             = _account->client()->capsManager();
    QString      client_name    = cm->clientName(caps_jid);
    QString      client_version = (client_name.isEmpty() ? QString() : cm->clientVersion(caps_jid));

    // make a resource so the contact appears online
    UserResource ur;
    ur.setName(nick);
    ur.setStatus(contact.status);
    ur.setClient(client_name, client_version, "");
    // ur.setClient(QString(),QString(),"");
    u.userResourceList().append(ur);
    u.setPrivate(true);
    u.setAvatarFactory(_account->avatarFactory());

    return u.makeTip();
}

QMimeData *GCUserModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *data = nullptr;
    if (!indexes.isEmpty()) {
        data = new QMimeData();
        data->setText(_selfJid.withResource(indexes.first().data().toString()).full());
    }

    return data;
}

Qt::DropActions GCUserModel::supportedDragActions() const { return Qt::CopyAction; }

QModelIndex GCUserModel::parent(const QModelIndex &child) const
{
    if (child.internalPointer()) { // contact
        auto c = static_cast<MUCContact *>(child.internalPointer());
        return index(groupRole(c->status), 0);
    }
    return QModelIndex();
}

int GCUserModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) { // amount of up-level groups
        return LastGroupRole;
    } else if (!parent.internalPointer() && parent.column() == 0) { // group
        if (parent.row() < LastGroupRole) {
            return contacts[parent.row()].size();
        }
    }
    return 0;
}

int GCUserModel::columnCount(const QModelIndex &parent) const
{
    if (parent.internalPointer()) { // contact
        return 0;
    }
    return 1;
}

void GCUserModel::removeEntry(const QString &nick)
{
    QModelIndex index = findIndex(nick);
    if (index.isValid()) {
        beginRemoveRows(index.parent(), index.row(), index.row());
        contacts[index.parent().row()].removeAt(index.row());
        endRemoveRows();
    }
    // TODO don't remove groups. just set display text to "" in data() (ex GCUserViewGroupItem::updateText)
}

GCUserModel::Role GCUserModel::groupRole(const Status &s)
{
    Role newGroupRole = Visitor;
    switch (s.mucItem().role()) {
    case MUCItem::Participant:
        newGroupRole = Participant;
        break;
    case MUCItem::Moderator:
        newGroupRole = Moderator;
        break;
    default:;
    }
    return newGroupRole;
}

void GCUserModel::updateEntry(const QString &nick, const Status &s)
{
    if (nick.isEmpty()) { // MUC self-presence? It should not come here
        return;
    }
    QModelIndex contactIndex = findIndex(nick);

    Role newGroupRole = groupRole(s);

    if (!contactIndex.isValid() || newGroupRole != contactIndex.parent().row()) {
        // either new contact or move between groups. we need to find destination position

        bool doStatusSort = PsiOptions::instance()->getOption("options.ui.muc.userlist.contact-sort-style").toString()
            == QLatin1String("status");
        int insertRowNum = 0;
        if (contacts[newGroupRole].size()) {
            // TODO use sorting filter model instad of code below.
            QString lowerNick = QLocale().toLower(nick);
            int     left = 0, right = contacts[newGroupRole].size();
            while (right - left > 0) { // std::lower_bound doesn't work here since we need index and not iterator
                int mid = (right + left) >> 1;

                int               rank;
                const MUCContact &contact = *(contacts[newGroupRole][mid]);
                if (doStatusSort) {
                    rank = rankStatus(s.type()) - rankStatus(contact.status.type());
                    if (rank == 0)
                        rank = QString::localeAwareCompare(lowerNick, QLocale().toLower(contact.name));
                } else {
                    rank = QString::localeAwareCompare(lowerNick, QLocale().toLower(contact.name));
                }

                if (rank <= 0) {
                    right = mid;
                } else {
                    left = mid + 1;
                }
            }
            insertRowNum = left;
        }

        QModelIndex newParentIndex = index(newGroupRole, 0);
        if (contactIndex.isValid()) { // move between group
            beginMoveRows(contactIndex.parent(), contactIndex.row(), contactIndex.row(), newParentIndex, insertRowNum);
            auto contact    = contacts[contactIndex.parent().row()].takeAt(contactIndex.row());
            contact->status = s;
            contacts[newGroupRole].insert(insertRowNum, contact);
            endMoveRows();
            // now report we want to change text of groups
            emit dataChanged(contactIndex.parent(), contactIndex.parent(),
                             QVector<int>() << Qt::DisplayRole); // TODO check if necessary
            emit dataChanged(newParentIndex, newParentIndex,
                             QVector<int>() << Qt::DisplayRole); // TODO check if necessary
        } else {                                                 // new contact
            emit beginInsertRows(newParentIndex, insertRowNum, insertRowNum);
            auto contact    = MUCContact::Ptr(new MUCContact);
            contact->name   = nick;
            contact->status = s;
            contact->avatar = _account->avatarFactory()->getMucAvatar(_selfJid.withResource(nick));
            contacts[newGroupRole].insert(insertRowNum, contact);
            if (nick == _selfJid.resource()) {
                _selfContact = contact;
            }
            emit endInsertRows();
        }
    } else {
        // just changed status. delegate will decide how to redraw properly
        auto contact    = contacts[contactIndex.parent().row()].at(contactIndex.row());
        contact->status = s;
        contact->avatar = _account->avatarFactory()->getMucAvatar(_selfJid.withResource(nick));
        emit dataChanged(contactIndex, contactIndex);
    }
}

void GCUserModel::clear()
{
    for (int i = LastGroupRole - 1; i >= 0; i--) {
        if (contacts[i].size()) {
            beginRemoveRows(index(i, 0), 0, contacts[i].size() - 1);
            contacts[i].clear();
            endRemoveRows();
        }
    }
}

void GCUserModel::updateAll()
{
    layoutAboutToBeChanged();
    // TODO sort contacts here? convert all icons to pixmaps for caching purposes?
    layoutChanged();
}

bool GCUserModel::hasJid(const Jid &jid)
{
    for (int gr = 0; gr < LastGroupRole; gr++) {
        for (auto const &c : contacts[gr]) {
            auto const &cj = c->status.mucItem().jid();
            if (!cj.isEmpty() && cj.compare(jid, false)) {
                return true;
            }
        }
    }
    return false;
}

QModelIndex GCUserModel::findIndex(const QString &nick) const
{
    for (int gr = 0; gr < LastGroupRole; gr++) {
        const QList<MUCContact::Ptr> &cs = contacts[gr];
        // it's possible to use binary search here since contacts are sorted by default.
        // but sorting algo is going to change to QString::localeAwareCompare and for binary search it's
        // quite heavy operation until we have really many contacts (maybe 200+).
        // so let's try plain search.
        for (int ci = 0; ci < cs.size(); ci++) {
            if (cs[ci]->name == nick) {
                return index(ci, 0, index(gr, 0));
            }
        }
    }

    return QModelIndex();
}

GCUserModel::MUCContact *GCUserModel::findEntry(const QString &nick) const
{
    return static_cast<GCUserModel::MUCContact *>(findIndex(nick).internalPointer());
}

QStringList GCUserModel::nickList() const
{
    QStringList nicks;
    for (int gr = 0; gr < LastGroupRole; gr++) {
        for (auto const &c : contacts[gr]) {
            nicks << c->name;
        }
    }
    nicks.sort(Qt::CaseInsensitive);
    return nicks;
}

//----------------------------------------------------------------------------
// GCUserView
//----------------------------------------------------------------------------

GCUserView::GCUserView(QWidget *parent) : QTreeView(parent)
{
    header()->hide();
    setRootIsDecorated(false);
    sortByColumn(0, Qt::AscendingOrder);
    setIndentation(0);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragDropMode(QAbstractItemView::DragOnly);

    setItemDelegate(new GCUserViewDelegate(this));
    // expandAll(); // doesn't work here

    connect(this, SIGNAL(doubleClicked(QModelIndex)), SLOT(qlv_doubleClicked(QModelIndex)));
}

GCUserView::~GCUserView() {}

void GCUserView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.parent().isValid()) {
        if (event->button() == Qt::MidButton
            || (event->button() == Qt::LeftButton && qApp->keyboardModifiers() == Qt::ShiftModifier)) {
            emit insertNick(index.data().toString());
            return;
        }
    }
    QTreeView::mousePressEvent(event);
}

void GCUserView::qlv_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || !index.parent().isValid())
        return;

    QString nick   = index.data().toString();
    Status  status = index.data(GCUserModel::StatusRole).value<Status>();
    if (PsiOptions::instance()->getOption(QLatin1String("options.messages.default-outgoing-message-type")).toString()
        == QLatin1String("message"))
        action(nick, status, 0); // message
    else
        action(nick, status, 1); // chat
}

void GCUserView::contextMenuEvent(QContextMenuEvent *cm)
{
    auto i = currentIndex();
    if (!i.isValid() || !i.parent().isValid()) { // not a contact
        QTreeView::contextMenuEvent(cm);
        return;
    }
    emit contextMenuRequested(i.data().toString());
}

void GCUserView::setLooks()
{
    static_cast<GCUserViewDelegate *>(itemDelegate())->updateSettings();
    viewport()->update();
}

#include "gcuserview.moc"
