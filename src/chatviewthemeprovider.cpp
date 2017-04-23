/*
 * chatviewthemeprovider.h - adapter for set of chatview themes
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
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
#if WEBENGINE
#include <QWebEngineUrlRequestInterceptor>
#endif

#include "chatviewtheme.h"
#include "psioptions.h"
#include "theme.h"
#include "applicationinfo.h"
#include "psithememanager.h"
#if WEBENGINE
# include "themeserver.h"
#endif
#include "chatviewthemeprovider_priv.h"

class ChatViewThemeProvider;

class ChatViewThemeUrlHandler : public NAMDataHandler
{
public:
	QByteArray data(const QUrl &url) const
	{
		qDebug("loading theme file: %s", qPrintable(url.toString()));
		QString themeId = url.path().section('/', 1, 2);
		ChatViewTheme *theme;
		if (!(theme = static_cast<ChatViewTheme *>(PsiThemeManager::instance()->provider("chatview")->current()))
				|| theme->id() != themeId) // slightly stupid detector of current provider
		{
			theme = static_cast<ChatViewTheme *>(PsiThemeManager::instance()->provider("groupchatview")->current());
			if (theme->id() != themeId) {
				theme = NULL;
			}
		}
		if (theme) {
			//theme->ChatViewTheme
			QByteArray td = theme->loadData(url.path().mid(themeId.size() + 2)); /* 2 slashes before and after */
			if (td.isNull()) {
				qDebug("content of %s is not found in the theme",
					   qPrintable(url.toString()));
			}
			return td;
		}
		qDebug("theme with id %s is not loaded. failed to load url %s",
			   qPrintable(themeId), qPrintable(url.toString()));
		return QByteArray();
	}

};

//--------------------------------------
// ChatViewThemeProvider
//--------------------------------------
ChatViewThemeProvider::ChatViewThemeProvider(QObject *parent_)
	: PsiThemeProvider(parent_)
	, curTheme(0)
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
Theme* ChatViewThemeProvider::theme(const QString &id)
{
	auto theme = new ChatViewTheme(this);
	theme->setId(id);
	if (theme->exists()) {
		return theme;
	}

	delete theme;
	return nullptr;
}

/**
 * @brief Load theme from settings or classic on failure.
 *  Signal themeChanged when necessary
 *
 * @return false on failure to load any theme
 */
bool ChatViewThemeProvider::loadCurrent()
{
	QString loadedId = curTheme? curTheme->id() : "";
	QString themeName = PsiOptions::instance()->getOption(optionString()).toString();
	if (!loadedId.isEmpty() && loadedId == themeName) {
		return true; // already loaded. nothing todo
	}
	ChatViewTheme *t = 0;
	if (!(t = (ChatViewTheme *)theme(themeName))) {
		if (themeName != QLatin1String("psi/classic")) {
			qDebug("Invalid theme id: %s", qPrintable(themeName));
			qDebug("fallback to classic chatview theme");
			PsiOptions::instance()->setOption(optionString(), QLatin1String("psi/classic"));
			return loadCurrent();
		}
		qDebug("Classic theme failed to load. No fallback..");
		return false;
	}

	bool startedLoading = t->load([this, t, loadedId](bool success){
		if (!success && t->id() != QLatin1String("psi/classic")) {
			qDebug("Failed to load theme \"%s\"", qPrintable(t->id()));
			qDebug("fallback to classic chatview theme");
			PsiOptions::instance()->setOption(optionString(), QLatin1String("psi/classic"));
			loadCurrent();
		} else if (success) {
			if (curTheme) {
				delete curTheme;
			}
			curTheme = t;
			if (t->id() != loadedId) {
				emit themeChanged();
			}
		} // else it was already classic
	});

	return startedLoading; // does not really matter. may fail later on loading
}

void ChatViewThemeProvider::setCurrentTheme(const QString &id)
{
	PsiOptions::instance()->setOption(optionString(), id);
	if (!curTheme || curTheme->id() != id) {
		loadCurrent();
	}
}

#if WEBENGINE
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
