/*
 * psiiconset.cpp - the Psi iconset class
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psiiconset.h"

#include "anim.h"
#include "applicationinfo.h"
#include "common.h"
#include "psievent.h"
#include "psioptions.h"
#include "userlist.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

using namespace XMPP;

struct ClientIconCheck
{
    QString icon; // icon name w/o client/ part
    QStringList inside; // search for texts inside provided name to be sure
};

/*
 * Main client icon search struct.
 * <left part of caps/clientName> =>  <list of icons with caps/clientName clarifications>
 *
 * Example:
 * client_icons.txt contents:
 *   psi-plus psi+,psi#fork#plus
 *   psi-ny psi#ny
 *
 * First column is icon name in the iconpack and remaining is a set of caps/clientName search spec.
 * This mean for clients with caps node starting with: psi+ and also for nodes starting with psi and having
 *   word "fork" or "plus" somewhere inside, "psi-plus" icons will be used. For psi-ny (New Year edition) icon
 *   caps whould start with "psi" and have "ny" somewhere in the middle.
 *
 * The structire below will look like
 *
 * {
 *   "psi"  => [
 *                {"psi-plus",["fork", "plus"]}
 *                {"psi-ny",["ny"]}
 *             ]
 *   "psi+" => [{"psi-plus",[]}]
 * }
 *
 * Now for example we need to lookup icon for caps node "psiplus.com". The most still mathing item here is "psi",
 * (psi+ won't match because psiplus.com doesn't start with psi+). And we don't have anything like "psip" or "psipl"..
 * So we review just "psi" (all its items consequently)
 * Both items in "psi" have clarification list. For the first item we take its clarification list ["fork", "plus"]
 * and review if any item is in "psiplus.com". The "plus" will be found, so the icon "psi-plus" will be returned.
 *
 * Note: It's quite regular for caps node to start with "https" but current client_icons.txt almost doesn't have
 * such records. It just means it heavily rely on detected client names instead of caps.
 * Client name from its side maybe taken from caps node when there is not other way to detect.
 * Example:
 * caps node = https://www.psi-im.org/helloworld/caps
 * resulting client name = psi-im.org/helloworld
 */
typedef QMap<QString, QList<ClientIconCheck> > ClientIconMap;

//----------------------------------------------------------------------------
// PsiIconset
//----------------------------------------------------------------------------

class PsiIconset::Private
{
private:
    PsiIconset *psi;
public:
    Iconset system, moods, clients, activities, affiliations;
    ClientIconMap client2icon;
    QString cur_system, cur_status, cur_moods, cur_clients, cur_activity, cur_affiliations;
    QStringList cur_emoticons;
    QMap<QString, QString> cur_service_status;
    QMap<QString, QString> cur_custom_status;
    struct StatusIconsets {
        struct IconsetItem {
            QRegExp regexp;
            QString iconset;
        };
        bool useServicesIcons = false;
        QList<IconsetItem> list;
        QList<IconsetItem> customList;
    } status_icons;

    Private(PsiIconset *_psi) {
        psi = _psi;
    }

    QString iconsetPath(QString name) {
        foreach (const QString &d, ApplicationInfo::dataDirs()) {
            QString fileName = d + "/iconsets/" + name;
            QFileInfo fi(fileName);
            if (fi.exists()) {
                return fileName;
            }
        }

        qWarning("PsiIconset::Private::iconsetPath(\"%s\"): not found", qPrintable(name));
        return QString();
    }

    void stripFirstAnimFrame(Iconset &is) {
        QListIterator<PsiIcon*> it = is.iterator();
        while (it.hasNext()) {
            it.next()->stripFirstAnimFrame();
        }
    }

    void loadIconset(Iconset *to, Iconset *from) {
        if ( !to ) {
            qWarning("PsiIconset::loadIconset(): 'to' iconset is NULL!");
            if ( from )
                qWarning("from->name() = '%s'", qPrintable(from->name()));
            return;
        }
        if ( !from ) {
            qWarning("PsiIconset::loadIconset(): 'from' iconset is NULL!");
            if ( to )
                qWarning("to->name() = '%s'", qPrintable(to->name()));
            return;
        }

        QListIterator<PsiIcon*> it = from->iterator();
        while( it.hasNext()) {
            PsiIcon *icon = it.next();

            if ( icon && !icon->name().isEmpty() ) {
                PsiIcon *toIcon = const_cast<PsiIcon *>(to->icon(icon->name()));
                if ( toIcon ) {
                    if ( icon->anim() ) {
                        // setAnim and setImpix both
                        //   emit pixmapChanged(),
                        //   however we only want this
                        //   to happen once, and only
                        //   after both functions have
                        //   been processed.  so we
                        //   block signals during the
                        //   first call.
                        bool b = toIcon->blockSignals(true);
                        toIcon->setAnim  ( *icon->anim(), false );
                        toIcon->blockSignals(b);
                        toIcon->setImpix ( icon->impix(), false );
                    }
                    else {
                        // same as above
                        bool b = toIcon->blockSignals(true);
                        toIcon->setAnim  ( Anim(),        false );
                        toIcon->blockSignals(b);
                        toIcon->setImpix ( icon->impix(), false );
                    }
                }
                else
                    to->setIcon( icon->name(), *icon );
            }
        }

        to->setInformation(*from);
    }

    PsiIcon *jid2icon(const Jid &jid, const QString &iconName)
    {
        // first level -- global default icon
        PsiIcon *icon = const_cast<PsiIcon *>(IconsetFactory::iconPtr(iconName));

        // second level -- transport icon
        if (jid.node().isEmpty() || status_icons.useServicesIcons) {
            foreach (const StatusIconsets::IconsetItem &item, status_icons.list) {
                if (item.regexp.isEmpty() ? jid.node().isEmpty() : (item.regexp.indexIn(jid.domain()) != -1)) {
                    const Iconset *is = psi->roster.value(item.iconset);
                    if (is) {
                        PsiIcon *i = const_cast<PsiIcon *>(is->icon(iconName));
                        if (i) {
                            icon = i;
                            break;
                        }
                    }
                }
            }
        }

        // third level -- custom icons
        foreach (const StatusIconsets::IconsetItem &item, status_icons.customList) {
            if (item.regexp.indexIn(jid.bare()) != -1) {
                const Iconset *is = psi->roster.value(item.iconset);
                if (is) {
                    PsiIcon *i = const_cast<PsiIcon *>(is->icon(iconName));
                    if (i) {
                        icon = i;
                        break;
                    }
                }
            }
        }

        return icon;
    }

    Iconset systemIconset(bool *ok)
    {
        Iconset def;
        *ok = def.load(":/iconsets/system/default");

        if ( PsiOptions::instance()->getOption("options.iconsets.system").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("system/" + PsiOptions::instance()->getOption("options.iconsets.system").toString()) );

            loadIconset(&def, &is);
        }

        stripFirstAnimFrame( def );

        return def;
    }

    Iconset *defaultRosterIconset(bool *ok)
    {
        Iconset *def = new Iconset;
        *ok = def->load (":/iconsets/roster/default");

        if ( PsiOptions::instance()->getOption("options.iconsets.status").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("roster/" + PsiOptions::instance()->getOption("options.iconsets.status").toString()) );

            loadIconset(def, &is);
        }

        stripFirstAnimFrame( *def );

        return def;
    }

    Iconset moodsIconset(bool *ok)
    {
        Iconset def;
        *ok = def.load( iconsetPath("moods/default") );

        if ( PsiOptions::instance()->getOption("options.iconsets.moods").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("moods/" + PsiOptions::instance()->getOption("options.iconsets.moods").toString()) );

            loadIconset(&def, &is);
        }

        stripFirstAnimFrame( def );

        return def;
    }

    Iconset activityIconset(bool *ok)
    {
        Iconset def;
        *ok = def.load( iconsetPath("activities/default") );

        if ( PsiOptions::instance()->getOption("options.iconsets.activities").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("activities/" + PsiOptions::instance()->getOption("options.iconsets.activities").toString()) );

            loadIconset(&def, &is);
        }

        stripFirstAnimFrame( def );

        return def;
    }

    Iconset clientsIconset(bool *ok)
    {
        Iconset def;
        *ok = def.load( iconsetPath("clients/default") );

        if ( PsiOptions::instance()->getOption("options.iconsets.clients").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("clients/" + PsiOptions::instance()->getOption("options.iconsets.clients").toString()) );

            loadIconset(&def, &is);
        }

        stripFirstAnimFrame( def );

        return def;
    }

    Iconset affiliationsIconset(bool *ok)
    {
        Iconset def;
        *ok = def.load( iconsetPath("affiliations/default") );

        if ( PsiOptions::instance()->getOption("options.iconsets.affiliations").toString() != "default" ) {
            Iconset is;
            is.load ( iconsetPath("affiliations/" + PsiOptions::instance()->getOption("options.iconsets.affiliations").toString()) );

            loadIconset(&def, &is);
        }

        stripFirstAnimFrame( def );

        return def;
    }

    QList<Iconset*> emoticons()
    {
        QList<Iconset*> emo;

        foreach(QString name, PsiOptions::instance()->getOption("options.iconsets.emoticons").toStringList()) {
            Iconset *is = new Iconset;
            if ( is->load ( iconsetPath("emoticons/" + name) ) ) {
                //PsiIconset::removeAnimation(is);
                is->addToFactory();
                emo.append( is );
            }
            else
                delete is;
        }

        return emo;
    }
};

PsiIconset::PsiIconset()
    : QObject(QCoreApplication::instance())
{
    d = new Private(this);
    d->status_icons.useServicesIcons = PsiOptions::instance()->getOption("options.ui.contactlist.use-transport-icons").toBool();
    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
    connect(PsiOptions::instance(), SIGNAL(destroyed()), SLOT(reset()));
}

PsiIconset::~PsiIconset()
{
    delete d;
}

void PsiIconset::reset()
{
    delete instance_;
    instance_ = nullptr;
    IconsetFactory::reset();
}

bool PsiIconset::loadSystem()
{
    bool ok = true;
    QString cur_system = PsiOptions::instance()->getOption("options.iconsets.system").toString();
    if (d->cur_system != cur_system) {
        Iconset sys = d->systemIconset(&ok);

        if(sys.iconSize() != d->system.iconSize()) {
            emit systemIconsSizeChanged(sys.iconSize());
        }

        d->loadIconset(&d->system, &sys);

        //d->system = d->systemIconset();
        d->system.addToFactory();

        d->cur_system = cur_system;
    }

    return ok;
}

bool PsiIconset::loadRoster()
{
    // load roster
    qDeleteAll(roster);
    roster.clear();

    // default roster iconset
    bool ok;
    Iconset *def = d->defaultRosterIconset(&ok);
    def->addToFactory();
    roster.insert(PsiOptions::instance()->getOption("options.iconsets.status").toString(), def);

    d->cur_status = PsiOptions::instance()->getOption("options.iconsets.status").toString();

    // load only necessary roster iconsets
    QSet<QString> rosterIconsets;
    d->cur_service_status.clear();

    foreach(QVariant service, PsiOptions::instance()->mapKeyList("options.iconsets.service-status")) {
        QString val = PsiOptions::instance()->getOption(
                          PsiOptions::instance()->mapLookup("options.iconsets.service-status", service) + ".iconset").toString();
        if (val.isEmpty())
            continue;
        rosterIconsets << val;
        d->cur_service_status.insert(service.toString(), val);
    }

    QStringList customicons = PsiOptions::instance()->getChildOptionNames("options.iconsets.custom-status", true, true);
    d->cur_custom_status.clear();
    foreach(QString base, customicons) {
        QString regexp = PsiOptions::instance()->getOption(base + ".regexp").toString();
        QString iconset = PsiOptions::instance()->getOption(base + ".iconset").toString();
        rosterIconsets << iconset;
        d->cur_custom_status.insert(regexp, iconset);
    }

    foreach(QString it2, rosterIconsets) {
        if (it2 == PsiOptions::instance()->getOption("options.iconsets.status").toString()) {
            continue;
        }

        Iconset *is = new Iconset;
        if (is->load(d->iconsetPath("roster/" + it2))) {
            is->addToFactory();
            d->stripFirstAnimFrame(*is);
            roster.insert(it2, is);
        }
        else {
            delete is;
        }
    }

    return ok;
}

void PsiIconset::loadEmoticons()
{
    QStringList cur_emoticons = PsiOptions::instance()->getOption("options.iconsets.emoticons").toStringList();
    if (d->cur_emoticons != cur_emoticons) {
        qDeleteAll(emoticons);
        emoticons.clear();
        emoticons = d->emoticons();

        d->cur_emoticons = cur_emoticons;
        emit emoticonsChanged();
    }
}

bool PsiIconset::loadMoods()
{
    bool ok = true;
    QString cur_moods = PsiOptions::instance()->getOption("options.iconsets.moods").toString();
    if (d->cur_moods != cur_moods) {
        Iconset moods = d->moodsIconset(&ok);
        d->loadIconset(&d->moods, &moods);
        d->moods.addToFactory();

        d->cur_moods = cur_moods;
    }

    return ok;
}

bool PsiIconset::loadActivity()
{
    bool ok = true;
    QString cur_activity = PsiOptions::instance()->getOption("options.iconsets.activities").toString();
    if (d->cur_activity != cur_activity) {
        Iconset activities = d->activityIconset(&ok);
        d->loadIconset(&d->activities, &activities);
        d->activities.addToFactory();

        d->cur_activity = cur_activity;
    }

    return ok;
}

bool PsiIconset::loadClients()
{
    bool ok = true;
    QString cur_clients = PsiOptions::instance()->getOption("options.iconsets.clients").toString();
    if (d->cur_clients != cur_clients) {
        Iconset clients = d->clientsIconset(&ok);
        d->loadIconset(&d->clients, &clients);
        d->clients.addToFactory();

        QSet<QString> iconsId;
        auto it = clients.iterator();
        while (it.hasNext()) {
            iconsId.insert(it.next()->name().section('/', 1, 1));
        }

        QStringList dirs = ApplicationInfo::dataDirs();
        ClientIconMap cm; // start part, spec[spec2[spec3]]
        foreach (const QString &dataDir, dirs) {
            QFile capsConv(dataDir + QLatin1String("/client_icons.txt"));
            /* file format: <icon res name> <left_part_of_cap1#inside1#inside2>,<left_part_of_cap2>
                next line the same.
            */
            if (capsConv.open(QIODevice::ReadOnly)) {
                QTextStream stream(&capsConv);

                QString line;
                while (!(line = stream.readLine()).isNull()) {
                    line = line.trimmed();
                    QString iconName = line.section(QLatin1Char(' '), 0, 0);
                    if (!iconName.length() || !iconsId.contains(iconName)) {
                        continue;
                    }
                    ClientIconCheck ic = {iconName, QStringList()};
                    QString caps = line.mid(iconName.length());
                    foreach (const QString &c, caps.split(QLatin1Char(','), QString::SkipEmptyParts)) {
                        QString ct = c.trimmed();
                        if (ct.length()) {
                            QStringList spec = ct.split('#');
                            if (spec.size() > 1) {
                                ic.inside = spec.mid(1);
                            } else {
                                ic.inside.clear();
                            }
                            ClientIconMap::Iterator it = cm.find(spec[0]);
                            if (it == cm.end()) {
                                cm.insert(spec[0], QList<ClientIconCheck>() << ic);
                            } else {
                                it.value().append(ic);
                            }
                        }
                    }
                }
                /* insert end boundry element to make search implementation simple */
                cm.insert(QLatin1String("~"), QList<ClientIconCheck>());
                break;
            }
        }
        if (cm.isEmpty()) {
            qWarning("Failed to read client_icons.txt. Clients detection won't work");
        }
        d->client2icon = cm;
        d->cur_clients = cur_clients;
    }

    return ok;
}

bool PsiIconset::loadAffiliations()
{
    bool ok = true;
    QString cur_affiliations = PsiOptions::instance()->getOption("options.iconsets.affiliations").toString();
    if (d->cur_affiliations != cur_affiliations) {
        Iconset affiliations = d->affiliationsIconset(&ok);
        d->loadIconset(&d->affiliations, &affiliations);
        d->affiliations.addToFactory();

        d->cur_affiliations = cur_affiliations;
    }

    return ok;
}

void PsiIconset::loadStatusIconDefinitions()
{
    d->status_icons.list.clear();
    d->status_icons.customList.clear();
    foreach(const QVariant& serviceV, PsiOptions::instance()->mapKeyList("options.iconsets.service-status")) {
        QString service = serviceV.toString();
        PsiIconset::Private::StatusIconsets::IconsetItem item;
        bool find = true;
        if (service == "aim")            item.regexp = QRegExp("^aim");
        else if (service == "disk")      item.regexp = QRegExp("^disk");
        else if (service == "gadugadu")  item.regexp = QRegExp("^gg");
        else if (service == "icq")       item.regexp = QRegExp("^icq|^jit");
        else if (service == "irc")       item.regexp = QRegExp("^irc");
        else if (service == "xmpp")      item.regexp = QRegExp("^j2j|^xmpp\\.[a-z1-9]+\\..*");
        else if (service == "mrim")      item.regexp = QRegExp("^mrim");
        else if (service == "msn")       item.regexp = QRegExp("^msn");
        else if (service == "muc")       item.regexp = QRegExp("^conference|^rooms");
        else if (service == "rss")       item.regexp = QRegExp("^rss");
        else if (service == "sms")       item.regexp = QRegExp("^sms");
        else if (service == "smtp")      item.regexp = QRegExp("^smtp");
        else if (service == "vkontakte") item.regexp = QRegExp("^vk.com|^vkontakte|^vk-t");
        else if (service == "weather")   item.regexp = QRegExp("^weather|^gism");
        else if (service == "yahoo")     item.regexp = QRegExp("^yahoo");
        else
            find = false;

        if (find) {
            item.iconset = PsiOptions::instance()->getOption(
                        PsiOptions::instance()->mapLookup("options.iconsets.service-status", service)
                        + ".iconset").toString();
            d->status_icons.list.append(item);
        }
    }
    // default transport icon set
    if (PsiOptions::instance()->mapKeyList("options.iconsets.service-status").contains("transport")) {
        PsiIconset::Private::StatusIconsets::IconsetItem item;
        item.iconset = PsiOptions::instance()->getOption(
                    PsiOptions::instance()->mapLookup("options.iconsets.service-status", "transport")
                    + ".iconset").toString();
        d->status_icons.list.append(item);
    }
    // custom icon sets
    foreach (const QString &base, PsiOptions::instance()->getChildOptionNames("options.iconsets.custom-status", true, true)) {
        PsiIconset::Private::StatusIconsets::IconsetItem item;
        item.regexp = QRegExp(PsiOptions::instance()->getOption(base + ".regexp").toString());
        if (item.regexp.isValid()) {
            item.iconset = PsiOptions::instance()->getOption(base + ".iconset").toString();
            d->status_icons.customList.append(item);
        }
    }
}

bool PsiIconset::loadAll()
{
    if (!loadSystem() || !loadRoster())
        return false;

    loadEmoticons();
    loadMoods();
    loadActivity();
    loadClients();
    loadAffiliations();
    loadStatusIconDefinitions();
    return true;
}

void PsiIconset::optionChanged(const QString& option)
{
    if (option == "options.iconsets.system") {
        loadSystem();
    }
    else if (option == "options.iconsets.emoticons") {
        loadEmoticons();
    }
    else if (option == "options.iconsets.moods") {
        loadMoods();
    }
    else if (option == "options.iconsets.activities") {
        loadActivity();
    }
    else if (option == "options.iconsets.clients") {
        loadClients();
    }
    else if (option == "options.iconsets.affiliations") {
        loadAffiliations();
    }
    else if (option == "options.ui.contactlist.use-transport-icons") {
        d->status_icons.useServicesIcons = PsiOptions::instance()->getOption("options.ui.contactlist.use-transport-icons").toBool();
    }

    // currently we rely on PsiCon calling reloadRoster() when
    // all options are already applied. otherwise we risk the chance
    // being called too many times

    // else if (option == "options.iconsets.status"                   ||
    //          option.startsWith("options.iconsets.service-status.") ||
    //          option.startsWith("options.iconsets.custom-status."))
    // {
    //     reloadRoster();
    // }
}

void PsiIconset::reloadRoster()
{
    bool ok;
    QString cur_status = PsiOptions::instance()->getOption("options.iconsets.status").toString();
    // default roster iconset
    if (d->cur_status != cur_status) {
        Iconset *newDef = d->defaultRosterIconset(&ok);
        Iconset *oldDef = roster[d->cur_status];

        if(oldDef->iconSize() != newDef->iconSize())
            emit rosterIconsSizeChanged(newDef->iconSize());

        d->loadIconset(oldDef, newDef);

        roster.remove(d->cur_status);

        roster.insert(cur_status, oldDef);
        delete newDef;
        d->cur_status = cur_status;
    }

    QMap<QString, QString> cur_service_status;
    QMap<QString, QString> cur_custom_status;

    foreach(QVariant service, PsiOptions::instance()->mapKeyList("options.iconsets.service-status")) {
        QString val = PsiOptions::instance()->getOption(
                          PsiOptions::instance()->mapLookup("options.iconsets.service-status", service) + ".iconset").toString();
        if (val.isEmpty())
            continue;
        cur_service_status.insert(service.toString(), val);
    }

    QStringList customicons = PsiOptions::instance()->getChildOptionNames("options.iconsets.custom-status", true, true);
    foreach(QString base, customicons) {
        QString regexp = PsiOptions::instance()->getOption(base + ".regexp").toString();
        QString iconset = PsiOptions::instance()->getOption(base + ".iconset").toString();
        cur_custom_status.insert(regexp, iconset);
    }

    // service&custom roster iconsets
    if (operator!=(d->cur_service_status, cur_service_status) || operator!=(d->cur_custom_status, cur_custom_status)) {
        QStringList rosterIconsets;

        QMap<QString, QString>::Iterator it = cur_service_status.begin();
        for (; it != cur_service_status.end(); ++it)
            if (!rosterIconsets.contains(it.value()))
                rosterIconsets << it.value();

        it = cur_custom_status.begin();
        for (; it != cur_custom_status.end(); ++it)
            if (!rosterIconsets.contains(it.value()))
                rosterIconsets << it.value();

        QStringList::Iterator it2 = rosterIconsets.begin();
        for (; it2 != rosterIconsets.end(); ++it2) {
            if (*it2 == PsiOptions::instance()->getOption("options.iconsets.status").toString())
                continue;

            Iconset *is = new Iconset;
            if (is->load(d->iconsetPath("roster/" + *it2))) {
                d->stripFirstAnimFrame(*is);
                Iconset *oldis = roster[*it2];

                if (oldis)
                    d->loadIconset(oldis, is);
                else {
                    is->addToFactory();
                    roster.insert(*it2, is);
                }
            }
            else
                delete is;
        }

        bool clear = false;
        while (!clear) {
            clear = true;

            QMutableHashIterator<QString, Iconset*> it3(roster);
            while (it3.hasNext()) {
                it3.next();
                QString name = it3.key();
                if (name == PsiOptions::instance()->getOption("options.iconsets.status").toString())
                    continue;

                if (!rosterIconsets.contains(name)) {
                    // remove redundant iconset
                    delete roster[name];
                    it3.remove();
                    clear = false;
                    break;
                }
            }
        }

        d->cur_service_status = cur_service_status;
        d->cur_custom_status  = cur_custom_status;
    }
}

PsiIcon *PsiIconset::event2icon(const PsiEvent::Ptr &e)
{
    QString icon;
    if(e->type() == PsiEvent::Message) {
        MessageEvent::Ptr me = e.staticCast<MessageEvent>();
        const Message &m = me->message();
        if(m.type() == "headline")
            icon = "psi/headline";
        else if(m.type() == "chat" || m.type() == "groupchat")
            icon = "psi/chat";
        else if(m.type() == "error")
            icon = "psi/system";
        else
            icon = "psi/message";
    }
    else if(e->type() == PsiEvent::File) {
        icon = "psi/file";
    }
    else if(e->type() == PsiEvent::AvCallType) {
        icon = "psi/call";
    }
    else {
        icon = "psi/system";
    }

    return d->jid2icon(e->from(), icon);
}

QString status2name(int s)
{
    QString name;
    switch ( s ) {
    case STATUS_OFFLINE:
        name = "status/offline";
        break;
    case STATUS_AWAY:
        name = "status/away";
        break;
    case STATUS_XA:
        name = "status/xa";
        break;
    case STATUS_DND:
        name = "status/dnd";
        break;
    case STATUS_INVISIBLE:
        name = "status/invisible";
        break;
    case STATUS_CHAT:
        name = "status/chat";
        break;

    case STATUS_ASK:
        name = "status/ask";
        break;
    case STATUS_NOAUTH:
        name = "status/noauth";
        break;
    case STATUS_ERROR:
        name = "status/error";
        break;

    case -1:
        name = "psi/connect";
        break;

    case STATUS_ONLINE:
    default:
        name = "status/online";
    }

    return name;
}

PsiIcon *PsiIconset::statusPtr(int s)
{
    return const_cast<PsiIcon *>(IconsetFactory::iconPtr(status2name(s)));
}

PsiIcon PsiIconset::status(int s)
{
    PsiIcon *icon = statusPtr(s);
    if ( icon )
        return *icon;
    return PsiIcon();
}

PsiIcon *PsiIconset::statusPtr(const XMPP::Status &s)
{
    return statusPtr(makeSTATUS(s));
}

PsiIcon PsiIconset::status(const XMPP::Status &s)
{
    return status(makeSTATUS(s));
}

PsiIcon *PsiIconset::transportStatusPtr(QString name, int s)
{
    PsiIcon *icon = nullptr;

    QVariantList serviceicons = PsiOptions::instance()->mapKeyList("options.iconsets.service-status");
    if (serviceicons.contains(name)) {
        const Iconset *is = roster.value(
                            PsiOptions::instance()->getOption(
                            PsiOptions::instance()->mapLookup("options.iconsets.service-status", name)+".iconset").toString());
        if ( is ) {
            icon = const_cast<PsiIcon *>(is->icon(status2name(s)));
        }
    }

    if ( !icon )
        icon = statusPtr(s);

    return icon;
}

PsiIcon *PsiIconset::transportStatusPtr(QString name, const XMPP::Status &s)
{
    return transportStatusPtr(name, makeSTATUS(s));
}

PsiIcon PsiIconset::transportStatus(QString name, int s)
{
    PsiIcon *icon = transportStatusPtr(name, s);
    if ( icon )
        return *icon;
    return PsiIcon();
}

PsiIcon PsiIconset::transportStatus(QString name, const XMPP::Status &s)
{
    PsiIcon *icon = transportStatusPtr(name, s);
    if ( icon )
        return *icon;
    return PsiIcon();
}

PsiIcon *PsiIconset::statusPtr(const XMPP::Jid &jid, int s)
{
    return d->jid2icon(jid, status2name(s));
}

PsiIcon *PsiIconset::statusPtr(const XMPP::Jid &jid, const XMPP::Status &s)
{
    return statusPtr(jid, makeSTATUS(s));
}

PsiIcon PsiIconset::status(const XMPP::Jid &jid, int s)
{
    PsiIcon *icon = statusPtr(jid, s);
    if ( icon )
        return *icon;
    return PsiIcon();
}

PsiIcon PsiIconset::status(const XMPP::Jid &jid, const XMPP::Status &s)
{
    PsiIcon *icon = statusPtr(jid, s);
    if ( icon )
        return *icon;
    return PsiIcon();
}

PsiIcon *PsiIconset::statusPtr(UserListItem *u)
{
    if ( !u )
        return nullptr;

    int s = 0;
    if ( !u->presenceError().isEmpty() )
        s = STATUS_ERROR;
    else if ( u->isTransport() ) {
        if ( u->isAvailable() )
            s = makeSTATUS( (*(u->priority())).status() );
        else
            s = STATUS_OFFLINE;
    }
    else if ( u->ask() == "subscribe" && !u->isAvailable() && !u->isTransport() )
        s = STATUS_ASK;
    else if ( (u->subscription().type() == Subscription::From || u->subscription().type() == Subscription::None) && !u->isAvailable() && !u->isPrivate() )
        s = STATUS_NOAUTH;
    else if( !u->isAvailable() )
        s = STATUS_OFFLINE;
    else
        s = makeSTATUS( (*(u->priority())).status() );

    return statusPtr(u->jid(), s);
}

PsiIcon PsiIconset::status(UserListItem *u)
{
    PsiIcon *icon = statusPtr(u);
    if ( icon )
        return *icon;
    return PsiIcon();
}

const Iconset &PsiIconset::system() const
{
    return d->system;
}

void PsiIconset::stripFirstAnimFrame(Iconset *is)
{
    if ( is )
        d->stripFirstAnimFrame(*is);
}

void PsiIconset::removeAnimation(Iconset *is)
{
    if ( is ) {
        QListIterator<PsiIcon*> it = is->iterator();
        while (it.hasNext()) {
            it.next()->removeAnim(false);
        }
    }
}

QString PsiIconset::caps2client(const QString &name)
{
    ClientIconMap::const_iterator it = d->client2icon.lowerBound(name);
    if (d->client2icon.size()) {
        // if name starts with found key or with key of previous item
        if ((it != d->client2icon.constEnd() && name.startsWith(it.key())) ||
                (it != d->client2icon.constBegin() && name.startsWith((--it).key()))) {
            foreach (const ClientIconCheck &ic, it.value()) {
                if (ic.inside.isEmpty()) {
                    return ic.icon;
                }
                bool matched = true;
                foreach (const QString &s, ic.inside) {
                    if (!name.contains(s)) {
                        matched = false;
                        break;
                    }
                }
                if (matched) {
                    return ic.icon;
                }
            }
        }
    }
    return QString();
}

PsiIconset* PsiIconset::instance()
{
    if (!instance_)
        instance_ = new PsiIconset();
    return instance_;
}

PsiIconset* PsiIconset::instance_ = nullptr;
