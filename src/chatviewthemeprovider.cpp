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
#include <QWebEngineUrlRequestInterceptor>

#include "chatviewtheme.h"
#include "psioptions.h"
#include "theme.h"
#include "applicationinfo.h"
#include "psiwkavatarhandler.h"
#include "psithememanager.h"
#if QT_WEBENGINEWIDGETS_LIB
# include "themeserver.h"
#endif
#include "chatviewthemeprovider_priv.h"

class ChatViewThemeProvider;

class ChatViewThemeUrlHandler : public NAMSchemeHandler
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
Theme * ChatViewThemeProvider::load(const QString &themeId)
{
	ChatViewTheme *theme = new ChatViewTheme(this);
	theme->load(themeId, [this, themeId](bool success){
		qDebug("%s theme loading status: %s", qPrintable(themeId), success? "success" : "failure");
		// TODO invent something smarter
	});
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
	QString loadedId = curTheme? curTheme->id() : "";
	QString themeName = PsiOptions::instance()->getOption(optionString()).toString();
	if (!loadedId.isEmpty() && loadedId == themeName) {
		return true; // already loaded. nothing todo
	}
	ChatViewTheme *theme = 0;
	if (!(theme = (ChatViewTheme *)load(themeName))) {
		if (themeName != "psi/classic") {
			qDebug("fallback to classic chatview theme");
			if ( (theme = (ChatViewTheme *)load("psi/classic")) ) {
				PsiOptions::instance()->setOption(optionString(), "psi/classic");
			}
		}
	}
	if (theme) {
		if (curTheme) {
			delete curTheme;
		}
		curTheme = theme;
		if (theme->id() != loadedId) {
			emit themeChanged();
		}
		return true;
	}
	return false;
}

void ChatViewThemeProvider::setCurrentTheme(const QString &id)
{
	PsiOptions::instance()->setOption(optionString(), id);
	if (!curTheme || curTheme->id() != id) {
		loadCurrent();
	}
}

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
