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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include "chatviewthemeprovider.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "chatviewtheme.h"
#include "psioptions.h"
#include "theme.h"
#include "applicationinfo.h"
#include "psiwkavatarhandler.h"
#include "psithememanager.h"

static bool singleInited = false;
static QMap<QString, QString> chatViewScripts;
static QMap<QString, QString> chatViewAdapterDirs;

class ChatViewThemeProvider;

class ChatViewThemeUrlHandler : public NAMSchemeHandler
{
public:
	QByteArray data(const QUrl &url) const
	{
		qDebug("loading theme file: %s", qPrintable(url.toString()));
		QString themeId = url.path().section('/', 1, 2);
		Theme *theme;
		if (!(theme = PsiThemeManager::instance()->provider("chatview")->current())
				|| theme->id() != themeId) // slightly stupid detector of current provider
		{
			theme = PsiThemeManager::instance()->provider("groupchatview")->current();
			if (theme->id() != themeId) {
				theme = NULL;
			}
		}
		if (theme) {
			QByteArray td = Theme::loadData(url.path().mid(themeId.size() + 1),
											theme->fileName());
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

static void singleInit(PsiCon *pc)
{
	if (!singleInited) {
		NetworkAccessManager::instance()->setSchemeHandler(
				"avatar", new PsiWKAvatarHandler(pc));
		NetworkAccessManager::instance()->setSchemeHandler(
				"theme", new ChatViewThemeUrlHandler());
		singleInited = true;
	}
}



//--------------------------------------
// ChatViewThemeProvider
//--------------------------------------
ChatViewThemeProvider::ChatViewThemeProvider(QObject *parent_)
	: PsiThemeProvider(parent_)
	, curTheme(0)
{
	singleInit((PsiCon*)parent());
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

Theme * ChatViewThemeProvider::load(const QString &themeId)
{
	QString up;
	if (chatViewScripts.value("util").isEmpty()) {
		if (!(up = themePath("chatview/util.js")).isEmpty()) {
			QFile file(up);
			if (file.open(QIODevice::ReadOnly)) {
				chatViewScripts["util"] = file.readAll();
			}
		}
	}
	if (!chatViewScripts.value("util").isEmpty()) {
		int pos;
		if ((pos = themeId.indexOf('/')) > 0) {
			QString tp = themePath("chatview/" + themeId);
			QString typeName = themeId.mid(0, pos);
			if (!tp.isEmpty()) { // theme exists
				QString ap, ad;
				if (chatViewScripts.value(typeName).isEmpty() &&
					!(ap = themePath("chatview/" + typeName + "/adapter.js")).isEmpty()) {
					QFile afile(ap);
					if (afile.open(QIODevice::ReadOnly)) {
						chatViewScripts[typeName] = afile.readAll();
						ad = QFileInfo(afile).dir().absolutePath();
						chatViewAdapterDirs.insert(typeName, ad);
					}
				} else {
					ad = chatViewAdapterDirs.value(typeName);
				}
				if (!ad.isEmpty() && !chatViewScripts.value(typeName).isEmpty()) {
					ChatViewTheme *theme = new ChatViewTheme(themeId);
					if (theme->load(tp, QStringList()<<chatViewScripts["util"]
									<<chatViewScripts[typeName], ad)) {
						return theme;
					}
				}
			}
		}
	}
	return 0;
}

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
