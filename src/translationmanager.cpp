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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "translationmanager.h"

#include "applicationinfo.h"
#include "psi_config.h"
#include "varlist.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QTranslator>

TranslationManager::TranslationManager()
{
    // Initialize
    currentLanguage_                             = "en";
    [[maybe_unused]] QString currentLanguageName = QT_TR_NOOP("language_name");

    // The application translator
    translator_ = new QTranslator(nullptr);

    // The qt translator
    qt_translator_ = new QTranslator(nullptr);

    // Self-destruct
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(deleteLater()));
}

TranslationManager::~TranslationManager()
{
    QCoreApplication::instance()->removeTranslator(translator_);
    delete translator_;
    translator_ = nullptr;

    QCoreApplication::instance()->removeTranslator(qt_translator_);
    delete qt_translator_;
    qt_translator_ = nullptr;
}

TranslationManager *TranslationManager::instance()
{
    if (!instance_) {
        instance_ = new TranslationManager();
    }
    return instance_;
}

const QString &TranslationManager::currentLanguage() const { return currentLanguage_; }

QString TranslationManager::currentXMLLanguage() const
{
    QString xmllang = currentLanguage_;
    xmllang.replace('_', "-");
    int at_index = xmllang.indexOf('@');
    if (at_index > 0)
        xmllang = xmllang.left(at_index);
    return xmllang;
}

bool loadQtTranslationHelper(const QString &language, const QString &dir, QTranslator *qt_translator)
{
    return qt_translator->load("qt_" + language, dir);
}

bool TranslationManager::loadQtTranslation(const QString &language)
{
    const auto &dirs = translationDirs();
    for (const QString &dir : dirs) {
        if (!QFile::exists(dir))
            continue;
        if (loadQtTranslationHelper(language, dir, qt_translator_)) {
            return true;
        }
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return loadQtTranslationHelper(language, QLibraryInfo::location(QLibraryInfo::TranslationsPath), qt_translator_);
#else
    return loadQtTranslationHelper(language, QLibraryInfo::path(QLibraryInfo::TranslationsPath), qt_translator_);
#endif
}

void TranslationManager::loadTranslation(const QString &language)
{
    // The default translation
    if (language == "en") {
        currentLanguage_ = language;
        // currentLanguageName_ = "English";
        QCoreApplication::instance()->removeTranslator(translator_);
        QCoreApplication::instance()->removeTranslator(qt_translator_);
        return;
    }

    // Try loading the translation file
    const auto &dirs = translationDirs();
    for (const QString &dir : dirs) {
        if (!QFile::exists(dir))
            continue;
        if (translator_->load("psi_" + language, dir)) {
            loadQtTranslation(language);

            if (currentLanguage_ == "en") {
                QCoreApplication::instance()->installTranslator(translator_);
                QCoreApplication::instance()->installTranslator(qt_translator_);
            }
            currentLanguage_ = language;
            break;
        }
    }
}

VarList TranslationManager::availableTranslations()
{
    VarList langs;

    // We always support english
    langs.set("en", "English");

    // Search the paths
    const auto &dirs = translationDirs();
    for (const QString &dirName : dirs) {
        if (!QFile::exists(dirName))
            continue;

        QDir        d(dirName);
        const auto &files = d.entryList();
        for (const QString &str : files) {
            // verify that it is a language file
            if (str.left(4) != "psi_")
                continue;
            int n = str.indexOf('.', 4);
            if (n == -1)
                continue;
            if (str.mid(n) != ".qm")
                continue;
            QString lang = str.mid(4, n - 4);

            // printf("found [%s], lang=[%s]\n", str.latin1(), lang.latin1());

            // get the language_name
            QString     name = QString("[") + str + "]";
            QTranslator t(nullptr);
            if (!t.load(str, dirName))
                continue;

            // Is translate equivalent to the old findMessage? I hope so
            // Qt4 conversion
            QString s = t.translate("@default", "language_name");
            if (!s.isEmpty())
                name = s;

            langs.set(lang, name);
        }
    }

    return langs;
}

QStringList TranslationManager::translationDirs() const
{
    static const QString &&subdir = "/translations";

#if defined(Q_OS_LINUX) && defined(SHARE_SUFF)
    // Special hack for correct work of AppImage, snap and flatpak builds
    static const QString &&additionalPath
        = QDir().absoluteFilePath(qApp->applicationDirPath() + "/../share/" SHARE_SUFF + subdir);
#endif

    static const QStringList &&dirs = {
#if defined(Q_OS_LINUX) && defined(SHARE_SUFF)
        additionalPath,
#endif
        ".",
        ApplicationInfo::homeDir(ApplicationInfo::DataLocation),
        ApplicationInfo::resourcesDir(),
        "." + subdir,
        ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + subdir,
        ApplicationInfo::resourcesDir() + subdir
    };
    return dirs;
}

TranslationManager *TranslationManager::instance_ = nullptr;
