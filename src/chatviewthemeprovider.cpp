/*
 * chatviewthemeprovider.cpp - adapter for set of chatview themes
 * Copyright (C) 2010-2017 Sergey Ilinykh
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */


#include "chatviewthemeprovider.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#ifdef WEBENGINE
#include <QWebEngineUrlRequestInterceptor>
#endif

#include "psicon.h"
#include "chatviewtheme.h"
#include "chatviewtheme_p.h"
#include "psioptions.h"
#include "theme.h"
#include "applicationinfo.h"
#include "psithememanager.h"
#ifdef WEBENGINE
# include "themeserver.h"
#endif
#include "chatviewthemeprovider_priv.h"
#include "chatviewtheme.h"

class ChatViewThemeProvider;

//--------------------------------------
// ChatViewThemeProvider
//--------------------------------------
ChatViewThemeProvider::ChatViewThemeProvider(PsiCon *psi) :
    PsiThemeProvider(psi)
{
	ChatViewCon::init((PsiCon*)parent());
}

const QStringList ChatViewThemeProvider::themeIds() const
{
	QStringList dirs;
	dirs << ":";
	dirs << ".";
	dirs << ApplicationInfo::homeDir(ApplicationInfo::DataLocation);
	dirs << ApplicationInfo::resourcesDir();

	QSet<QString> ret;
	foreach (QString dir, dirs) {
		foreach (QFileInfo tDirInfo, QDir(dir+"/themes/chatview/")
			.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
			QString typeName = tDirInfo.fileName();
			foreach (QFileInfo themeInfo,
					QDir(tDirInfo.absoluteFilePath())
						.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot) +
					QDir(tDirInfo.absoluteFilePath())
						.entryInfoList(QStringList("*.theme"), QDir::Files)) {
				ret<<(QString("%1/%2").arg(typeName).arg(themeInfo.fileName()));
				//qDebug("found theme: %s", qPrintable(QString("%1/%2").arg(typeName).arg(themeInfo.fileName())));
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
 * @brief Load theme from settings or classic on failure.
 *  Signal themeChanged when necessary
 *
 * @return false on failure to load any theme
 */
bool ChatViewThemeProvider::loadCurrent()
{
	QString loadedId = curTheme.id();
	QString themeName = PsiOptions::instance()->getOption(optionString()).toString();
	if (!loadedId.isEmpty() && loadedId == themeName) {
		return true; // already loaded. nothing todo
	}
	Theme t(theme(themeName));
	if (!t.exists()) {
		if (themeName != QLatin1String("psi/classic")) {
			qDebug("Invalid theme id: %s", qPrintable(themeName));
			qDebug("fallback to classic chatview theme");
			PsiOptions::instance()->setOption(optionString(), QLatin1String("psi/classic"));
			return loadCurrent();
		}
		qDebug("Classic theme failed to load. No fallback..");
		return false;
	}

	bool startedLoading = t.load([this, t, loadedId](bool success){
		if (!success && t.id() != QLatin1String("psi/classic")) {
			qDebug("Failed to load theme \"%s\"", qPrintable(t.id()));
			qDebug("fallback to classic chatview theme");
			PsiOptions::instance()->setOption(optionString(), QLatin1String("psi/classic"));
			loadCurrent();
		} else if (success) {
			curTheme = t;
			if (t.id() != loadedId) {
				emit themeChanged();
			}
		} // else it was already classic
	});

	return startedLoading; // does not really matter. may fail later on loading
}

Theme ChatViewThemeProvider::current() const
{
	return curTheme;
}

void ChatViewThemeProvider::setCurrentTheme(const QString &id)
{
	PsiOptions::instance()->setOption(optionString(), id);
	if (!curTheme.isValid() || curTheme.id() != id) {
		loadCurrent();
	}
}

#ifdef WEBENGINE
ThemeServer *ChatViewThemeProvider::themeServer()
{
	Q_ASSERT(ChatViewCon::isReady());
	return ChatViewCon::instance()->themeServer;
}

QWebEngineUrlRequestInterceptor *ChatViewThemeProvider::requestInterceptor()
{
	Q_ASSERT(ChatViewCon::isReady());
	return ChatViewCon::instance()->requestInterceptor;
}
#endif
