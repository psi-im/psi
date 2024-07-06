/*
 * chatviewthemeprovider.cpp - adapter for set of chatview themes
 * Copyright (C) 2010-2017  Sergey Ilinykh
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

#include "chatviewthemeprovider.h"

#include "applicationinfo.h"
#include "chatviewtheme.h"
#include "chatviewtheme_p.h"
#include "chatviewthemeprovider_priv.h"
#include "psicon.h"
#include "psioptions.h"
#include "psithememanager.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#ifdef WEBENGINE
#include <QWebEngineUrlRequestInterceptor>
#endif

class ChatViewThemeProvider;

//--------------------------------------
// ChatViewThemeProvider
//--------------------------------------
ChatViewThemeProvider::ChatViewThemeProvider(PsiCon *psi) : PsiThemeProvider(psi)
{
    ChatViewCon::init(static_cast<PsiCon *>(parent()));
}

const QStringList ChatViewThemeProvider::themeIds() const
{
    QStringList dirs;
    dirs << ":";
    dirs << ".";
    dirs << ApplicationInfo::homeDir(ApplicationInfo::DataLocation);
    dirs << ApplicationInfo::resourcesDir();

    QSet<QString> ret;
    for (const QString &dir : std::as_const(dirs)) {
        foreach (QFileInfo tDirInfo,
                 QDir(dir + "/themes/chatview/").entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
            QString typeName = tDirInfo.fileName();
            foreach (QFileInfo themeInfo,
                     QDir(tDirInfo.absoluteFilePath()).entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)
                         + QDir(tDirInfo.absoluteFilePath())
                               .entryInfoList(QStringList { { "*.theme", "*.zip" } }, QDir::Files)) {
                ret << (QString("%1/%2").arg(typeName, themeInfo.fileName()));
                // qDebug("found theme: %s", qPrintable(QString("%1/%2").arg(typeName).arg(themeInfo.fileName())));
            }
        }
    }

    return ret.values();
}

/**
 * @brief Caches all the theme resources in the memory
 *  Reads metadata.
 *
 * @param themeId theme to load
 * @return
 */
Theme ChatViewThemeProvider::theme(const QString &id)
{
    Theme theme(new ChatViewThemePrivate(this));
    theme.setId(id);
    return theme;
}

/**
 * @brief Load theme from settings or New Classic on failure.
 *  Signals themeChanged if different theme was loaded before.
 *
 * @return false on failure to load any theme
 */
PsiThemeProvider::LoadRestult ChatViewThemeProvider::loadCurrent()
{
    QString loadedId    = curTheme.id();
    QString loadedStyle = loadedId.isEmpty() ? loadedId : curTheme.style();
    QString themeName   = PsiOptions::instance()->getOption(optionString()).toString();
    QString style       = PsiOptions::instance()->getOption(optionString() + QLatin1String("-style")).toString();
    if (!loadedId.isEmpty() && loadedId == themeName && loadedStyle == style) {
        return LoadSuccess; // already loaded. nothing todo
    }
    auto newClassic = QStringLiteral("psi/new_classic");
    curLoadingTheme = theme(themeName); // may trigger destructor of prev curLoadingTheme
    if (!curLoadingTheme.exists()) {
        if (themeName != newClassic) {
            qDebug("Invalid theme id: %s", qPrintable(themeName));
            qDebug("fallback to new_classic chatview theme");
            PsiOptions::instance()->setOption(optionString(), newClassic);
            style.clear();
            curLoadingTheme = theme(newClassic);
        } else {
            qDebug("New Classic theme failed to load. No fallback..");
            return LoadFailure;
        }
    }

    bool startedLoading = curLoadingTheme.load(style, [this, loadedId, loadedStyle, newClassic](bool success) {
        auto t          = curLoadingTheme;
        curLoadingTheme = {};
        if (success) {
            curTheme = t;
            if (t.id() != loadedId || t.style() != loadedStyle) {
                emit themeChanged();
            }
            return;
        }
        if (t.id() != newClassic) {
            qDebug("Failed to load theme \"%s\"", qPrintable(t.id()));
            qDebug("fallback to classic chatview theme");
            PsiOptions::instance()->setOption(optionString(), newClassic);
            loadCurrent();
        } else {
            // nowhere to fallback
            emit currentLoadFailed();
        }
    });

    if (startedLoading) {
        return LoadInProgress; // does not really matter. may fail later on loading
    }
    return LoadFailure;
}

void ChatViewThemeProvider::unloadCurrent() { curTheme = Theme(); }

void ChatViewThemeProvider::cancelCurrentLoading() { curLoadingTheme = {}; /* should call destructor */ }

Theme ChatViewThemeProvider::current() const { return curTheme; }

void ChatViewThemeProvider::setCurrentTheme(const QString &id, const QString &style)
{
    PsiOptions::instance()->setOption(optionString(), id);
    PsiOptions::instance()->setOption(optionString() + QLatin1String("-style"), style);
    if (!curTheme.isValid() || curTheme.id() != id || curTheme.style() != style) {
        loadCurrent();
    }
}

#ifdef WEBENGINE
QWebEngineUrlRequestInterceptor *ChatViewThemeProvider::requestInterceptor()
{
    Q_ASSERT(ChatViewCon::isReady());
    return ChatViewCon::instance()->requestInterceptor;
}
#endif
