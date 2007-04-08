/*
 * translationmanager.cpp
 * Copyright (C) 2006  Remko Troncon, Justin Karneges
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

#include <QTranslator>
#include <QCoreApplication>
#include <QFile>
#include <QDir>

#include "translationmanager.h"
#include "applicationinfo.h"

TranslationManager::TranslationManager()
{
	// Initialize
	currentLanguage_ = "en";
	//currentLanguageName_ = QT_TR_NOOP("language_name");

	// The application translator
	translator_ = new QTranslator(0);
	QCoreApplication::instance()->installTranslator(translator_);

	// The qt translator
	qt_translator_ = new QTranslator(0);
	QCoreApplication::instance()->installTranslator(qt_translator_);
	
	// Self-destruct
	connect(QCoreApplication::instance(),SIGNAL(aboutToQuit()),SLOT(deleteLater()));
}

TranslationManager::~TranslationManager()
{
	QCoreApplication::instance()->removeTranslator(translator_);
	delete translator_;
	translator_ = 0;
	
	QCoreApplication::instance()->removeTranslator(qt_translator_);
	delete qt_translator_;
	qt_translator_ = 0;
}

TranslationManager* TranslationManager::instance() 
{
	if (!instance_) {
		instance_ = new TranslationManager();
	}
	return instance_;
}

const QString& TranslationManager::currentLanguage() const
{
	return currentLanguage_;
}

QString TranslationManager::currentXMLLanguage() const
{
	QString xmllang = currentLanguage_;
	xmllang.replace('_',"-");
	int at_index = xmllang.find('@');
	if (at_index > 0)
		xmllang = xmllang.left(at_index);
	return xmllang;
}

void TranslationManager::loadTranslation(const QString& language)
{
#ifdef __GNUC__
#warning "The translation needs to be reset in case 'english' is selected
#endif
	//printf("changing lang: [%s]\n", lang.latin1());
	//The Qt book suggests these are not necessary and they don't
  	//exist in Qt4
  	/*trans->clear();
	qttrans->clear();*/

	// The default translation
	if(language == "en") {
		currentLanguage_ = language;
		//currentLanguageName_ = "English";
		return;
	}
	
	// Try loading the translation file
	QStringList dirs = translationDirs();
	for(QStringList::Iterator it = dirs.begin(); it != dirs.end(); ++it) {
		if(!QFile::exists(*it))
			continue;
		if (translator_->load("psi_" + language, *it)) {
			// try to load qt library translation
			qt_translator_->load("qt_" + language, *it);
			currentLanguage_ = language;
		}
	}
}

VarList TranslationManager::availableTranslations()
{
	VarList langs;

	// We always support english
	langs.set("en", "English");
	
	// Search the paths
	QStringList dirs = TranslationManager::translationDirs();
	for(QStringList::Iterator it = dirs.begin(); it != dirs.end(); ++it) {
		if(!QFile::exists(*it))
			continue;
		QDir d(*it);
		QStringList entries = d.entryList();
		for(QStringList::Iterator it2 = entries.begin(); it2 != entries.end(); ++it2) {
			if(*it2 == "." || *it2 == "..")
				continue;

			QString str = *it2;
			// verify that it is a language file
			if(str.left(4) != "psi_")
				continue;
			int n = str.find('.', 4);
			if(n == -1)
				continue;
			if(str.mid(n) != ".qm")
				continue;
			QString lang = str.mid(4, n-4);

			//printf("found [%s], lang=[%s]\n", str.latin1(), lang.latin1());

			// get the language_name
			QString name = QString("[") + str + "]";
			QTranslator t(0);
			if(!t.load(str, *it))
				continue;

			//Is translate equivalent to the old findMessage? I hope so
			//Qt4 conversion
			QString s = t.translate("@default", "language_name");
			if(!s.isEmpty())
				name = s;

			langs.set(lang, name);
		}
	}
	
	return langs;
}

QStringList TranslationManager::translationDirs() const
{
	QStringList dirs;
	QString subdir = "";
	dirs += "." + subdir;
	dirs += ApplicationInfo::homeDir() + subdir;
	dirs += ApplicationInfo::resourcesDir() + subdir;
	return dirs;
}


TranslationManager* TranslationManager::instance_ = NULL;
