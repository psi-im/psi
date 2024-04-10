/*
 * urlobject.cpp - helper class for handling links
 * Copyright (C) 2003-2006  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "urlobject.h"

#include "iconaction.h"
#ifndef WIDGET_PLUGIN
#include "psioptions.h"
#endif

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QSignalMapper>
#include <QUrl>
#include <QUrlQuery>

//! \if _hide_doc_
class URLObject::Private : QObject {
    Q_OBJECT
public:
    QString     link;
    IconAction *act_xmpp, *act_mailto, *act_join_groupchat, *act_send_message, *act_chat, *act_browser,
        *act_add_to_roster, *act_copy, *act_info;
    URLObject *urlObject;

    void connectXmppAction(QAction *action, const QString &query)
    {
        connect(action, &QAction::triggered, this, [query, this]() { xmppAction(query); });
    }

    Private(URLObject *parent) : QObject(parent)
    {
        urlObject = parent;
        QString tr;

        tr       = qApp->translate("URLLabel", "Open");
        act_xmpp = new IconAction(tr, "psi/jabber", tr, 0, this);
        connectXmppAction(act_xmpp, "");

        tr         = qApp->translate("URLLabel", "Open mail composer");
        act_mailto = new IconAction(tr, "psi/email", tr, 0, this);
        connect(act_mailto, SIGNAL(triggered()), SLOT(popupAction()));

        tr          = qApp->translate("URLLabel", "Open web browser");
        act_browser = new IconAction(tr, "psi/www", tr, 0, this);
        connect(act_browser, SIGNAL(triggered()), SLOT(popupAction()));

        tr                = qApp->translate("URLLabel", "Add to Roster");
        act_add_to_roster = new IconAction(tr, "psi/addContact", tr, 0, this);
        connectXmppAction(act_add_to_roster, "roster");

        tr               = qApp->translate("URLLabel", "Send message to");
        act_send_message = new IconAction(tr, "psi/message", tr, 0, this);
        connectXmppAction(act_send_message, "message");

        tr       = qApp->translate("URLLabel", "Chat with");
        act_chat = new IconAction(tr, "psi/chat", tr, 0, this);
        connectXmppAction(act_chat, "message;type=chat");

        tr                 = qApp->translate("URLLabel", "Join groupchat");
        act_join_groupchat = new IconAction(tr, "psi/groupChat", tr, 0, this);
        connectXmppAction(act_join_groupchat, "join");

        tr       = qApp->translate("URLLabel", "Copy location");
        act_copy = new IconAction(tr, tr, 0, this);
        connect(act_copy, SIGNAL(triggered()), SLOT(popupCopy()));

        tr       = qApp->translate("URLLabel", "User Info");
        act_info = new IconAction(tr, "psi/vCard", tr, 0, this);
        connectXmppAction(act_info, "vcard");
    }

    QString copyString(QString from)
    {
        QString l = from;

        int colon = l.indexOf(':');
        if (colon == -1)
            colon = 0;
        QString service = l.left(colon);

        if (service == "mailto" || service == "jabber" || service == "jid" || service == "xmpp"
            || service == "x-psi-atstyle") {
            if (colon > -1)
                l = l.mid(colon + 1);

            while (l[0] == '/')
                l = l.mid(1);
        }

        return l;
    }

public slots:
    void popupAction(QString lnk)
    {
        if (lnk.startsWith("x-psi-atstyle:")) {
            lnk.replace(0, 13, "xmpp");
        }
        emit urlObject->openURL(lnk);
    }

    void popupAction() { popupAction(link); }

    void popupCopy(QString lnk)
    {
        QApplication::clipboard()->setText(copyString(lnk), QClipboard::Clipboard);
        if (QApplication::clipboard()->supportsSelection())
            QApplication::clipboard()->setText(copyString(lnk), QClipboard::Selection);
    }

    void popupCopy() { popupCopy(link); }

    void xmppAction(const QString &lnk, const QString &query)
    {
        QUrl uri(lnk);
        if (!query.isEmpty()) {
            QString   queryType = query.left(query.indexOf(';'));
            QUrlQuery q;
            q.setQueryDelimiters('=', ';');
            q.setQuery(uri.query(QUrl::FullyEncoded));

            if (q.queryItems().value(0).first != queryType) {
                q.setQuery(query);
            }
            uri.setQuery(q);
        }
        uri.setScheme("xmpp");
        emit urlObject->openURL(uri.toString());
    }

    void xmppAction(const QString &query) { xmppAction(link, query); }
};
//! \endif

/**
 * \class URLObject
 * \brief Helper class for handling clicking on URLs
 */

/**
 * Default constructor.
 */
URLObject::URLObject() : QObject(qApp) { d = new Private(this); }

/**
 * Returns instance of the class, and creates it if necessary.
 */
URLObject *URLObject::getInstance()
{
    static URLObject *urlObject = nullptr;
    if (!urlObject)
        urlObject = new URLObject();
    return urlObject;
}

/**
 * Creates QMenu with actions corresponding to link's type.
 * @param lnk link in service:url format
 */
QMenu *URLObject::createPopupMenu(const QString &lnk)
{
    d->link = lnk;
    if (d->link.isEmpty())
        return nullptr;

    int colon = d->link.indexOf(':');
    if (colon == -1)
        colon = 0;
    QString service = d->link.left(colon);

    QMenu *m = new QMenu;

    bool needGenericOpen = true;
    if (service == "jabber" || service == "jid" || service == "xmpp" || service == "x-psi-atstyle") {
        needGenericOpen = false;
        m->addAction(d->act_xmpp);
        m->addAction(d->act_chat);
        m->addAction(d->act_send_message);
        m->addAction(d->act_join_groupchat);
        m->addAction(d->act_add_to_roster);
        m->addAction(d->act_info);
        if (service == "x-psi-atstyle") {
            m->addSeparator();
        }
    }
    if (service == "mailto" || service == "x-psi-atstyle") {
        needGenericOpen = false;
        m->addAction(d->act_mailto);
        if (service == "x-psi-atstyle") {
            m->addSeparator();
        }
    }
    if (needGenericOpen) {
        m->addAction(d->act_browser);
    }

    m->addAction(d->act_copy);
#ifndef WIDGET_PLUGIN
    m->setStyleSheet(PsiOptions::instance()->getOption("options.ui.look.css").toString());
#endif
    return m;
}

/**
 * Simulates default action using the passed URL.
 * \param lnk URL string
 */
void URLObject::popupAction(QString lnk) { d->popupAction(lnk); }

#include "urlobject.moc"
