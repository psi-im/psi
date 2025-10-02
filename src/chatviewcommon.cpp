/*
 * chatviewcommon.cpp - shared part of any chatview
 * Copyright (C) 2010  Sergey Ilinykh
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

#include "chatviewcommon.h"

#include "psioptions.h"

#include <QApplication>
#include <QRegularExpression>
#include <QSet>
#include <QWidget>
#include <math.h>

void ChatViewCommon::setLooks(QWidget *w)
{
    QPalette pal = w->palette(); // copy widget's palette to non const QPalette
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, pal.color(QPalette::Active, QPalette::HighlightedText));
    pal.setColor(QPalette::Inactive, QPalette::Highlight, pal.color(QPalette::Active, QPalette::Highlight));
    w->setPalette(pal); // set the widget's palette
}

bool ChatViewCommon::updateLastMsgTime(QDateTime t)
{
    bool doInsert = t.date() != _lastMsgTime.date();
    if (!_lastMsgTime.isValid() || t > _lastMsgTime) {
        _lastMsgTime = t;
    }
    return doInsert;
}

QString ChatViewCommon::getMucNickColor(const QString &nick, bool isSelf)
{
    do {
        if (!PsiOptions::instance()->getOption("options.ui.muc.use-nick-coloring").toBool()) {
            break;
        }

        QString nickwoun = nick; // nick without underscores
        nickwoun.remove(QRegularExpression("(^_*|_*$)"));

        if (PsiOptions::instance()->getOption("options.ui.muc.use-hash-nick-coloring").toBool()) {
            /* Hash-driven colors */
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            quint32 hash = qHash(nickwoun); // almost unique hash
#else
            size_t hash = qHash(nickwoun); // almost unique hash
#endif
            QList<QColor> &_palette = generatePalette();
            return _palette.at(int(hash % uint(_palette.size()))).name();
        }

        QStringList nickColors
            = PsiOptions::instance()->getOption("options.ui.look.colors.muc.nick-colors").toStringList();

        if (nickColors.empty()) {
            break;
        }

        if (isSelf || nickwoun.isEmpty() || nickColors.size() == 1) {
            return nickColors[0];
        }
        QMap<QString, int>::iterator it = _nicks.find(nickwoun);
        if (it == _nicks.end()) {
            // not found in map
            it = _nicks.insert(nickwoun, _nickNumber);
            _nickNumber++;
        }
        return nickColors[it.value() % (nickColors.size() - 1)];
    } while (false);

    return qApp->palette().color(QPalette::Inactive, QPalette::WindowText).name();
}

void ChatViewCommon::addUser(const QString &nickname) { }

void ChatViewCommon::removeUser(const QString &nickname) { }

void ChatViewCommon::renameUser(const QString &oldNickname, const QString &newNickname) { }

QList<QColor> &ChatViewCommon::generatePalette()
{
    static QColor        bg;
    static QList<QColor> result;

    QColor cbg = qApp->palette().color(QPalette::Base); // background color
    if (result.isEmpty() || cbg != bg) {
        result.clear();
        bg = cbg;
        QColor color;
        for (int k = 0; k < 10; ++k) {
            color = QColor::fromHsv(36 * k, 255, 255);
            if (compatibleColors(color, bg)) {
                result << color;
            }
            color = QColor::fromHsv(36 * k, 255, 170);
            if (compatibleColors(color, bg)) {
                result << color;
            }
        }
    }
    return result;
}

bool ChatViewCommon::compatibleColors(const QColor &c1, const QColor &c2)
{
    int dR = c1.red() - c2.red();
    int dG = c1.green() - c2.green();
    int dB = c1.blue() - c2.blue();

    double dV = abs(c1.value() - c2.value());
    double dC = sqrt(0.2126 * dR * dR + 0.7152 * dG * dG + 0.0722 * dB * dB);

    return !((dC < 80. && dV > 100) || (dC < 110. && dV <= 100 && dV > 10) || (dC < 125. && dV <= 10));
}

QList<ChatViewCommon::ReactionsItem>
ChatViewCommon::updateReactions(const QString &senderNickname, const QString &messageId, const QSet<QString> &reactions)
{
    auto          msgIt = _reactions.find(messageId);
    QSet<QString> toAdd = reactions;
    QSet<QString> toRemove;

    QHash<QString, QSet<QString>>::Iterator userIt;
    if (msgIt != _reactions.end()) {
        auto &sotredReactions = msgIt.value();
        userIt                = sotredReactions.perUser.find(senderNickname);
        if (userIt != sotredReactions.perUser.end()) {
            toAdd    = reactions - userIt.value();
            toRemove = userIt.value() - reactions;
        } else {
            userIt = sotredReactions.perUser.insert(senderNickname, {});
        }
    } else {
        msgIt  = _reactions.insert(messageId, {});
        userIt = msgIt.value().perUser.insert(senderNickname, {});
    }
    *userIt = reactions;
    for (auto const &v : std::as_const(toAdd)) {
        msgIt.value().total[v].append(senderNickname);
    }
    for (auto const &v : std::as_const(toRemove)) {
        auto it = msgIt.value().total.find(v);
        it->removeOne(senderNickname);
        if (it->isEmpty()) {
            msgIt.value().total.erase(it);
        }
    }
    auto &total = msgIt.value().total;

    QList<ReactionsItem> ret;
    for (auto it = total.begin(); it != total.end(); ++it) {
        static const auto skinRemove = QRegularExpression("([\\x{1F3FB}-\\x{1F3FF}]|[\\x{1F9B0}-\\x{1F9B2}])");
        QString           orig       = it.key();
        auto              sanitized  = orig.remove(skinRemove);
        ret << ReactionsItem { sanitized != orig ? sanitized : QString {}, orig, it.value() };
    }
    if (total.isEmpty()) {
        _reactions.erase(msgIt);
    } else if (userIt->isEmpty()) {
        msgIt.value().perUser.erase(userIt);
    }
    return ret;
}

QSet<QString> ChatViewCommon::onReactionSwitched(const QString &senderNickname, const QString &messageId,
                                                 const QString &reaction)
{
    auto msgIt = _reactions.find(messageId);
    if (msgIt == _reactions.end()) {
        return { { reaction } };
    }
    auto userIt = msgIt->perUser.find(senderNickname);
    if (userIt == msgIt->perUser.end()) {
        return { { reaction } };
    }
    auto ret = *userIt;
    if (ret.contains(reaction)) {
        ret.remove(reaction);
    } else {
        ret.insert(reaction);
    }
    return ret;
}
