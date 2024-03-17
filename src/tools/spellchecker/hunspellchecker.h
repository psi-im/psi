/*
 * hunspellchecker.h
 *
 * Copyright (C) 2015  Sergey Ilinykh, Vitaly Tonkacheyev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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
#ifndef HUNSPELLCHECKER_H
#define HUNSPELLCHECKER_H

#include "languagemanager.h"
#include "spellchecker.h"

#include <QFileInfo>
#include <QList>
#include <QLocale>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#include <QStringEncoder>
#endif

class Hunspell;
class QTextCodec;

typedef QSharedPointer<Hunspell> HunspellPtr;

class HunspellChecker : public SpellChecker {
public:
    HunspellChecker();
    ~HunspellChecker();
    virtual QList<QString>                suggestions(const QString &);
    virtual bool                          isCorrect(const QString &word);
    virtual bool                          add(const QString &word);
    virtual bool                          available() const;
    virtual bool                          writable() const;
    virtual void                          setActiveLanguages(const QSet<LanguageManager::LangId> &langs);
    virtual QSet<LanguageManager::LangId> getAllLanguages() const;

private:
    struct DictInfo {
        LanguageManager::LangId langId;
        QString                 filename;
    };
    struct LangItem {
        HunspellPtr hunspell_;
        DictInfo    info;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QTextCodec *codec;
#else
        QStringEncoder encoder;
        QStringDecoder decoder;
#endif
    };
    void getSupportedLanguages();
    void addLanguage(const LanguageManager::LangId &langId);
    void getDictPaths();
    bool scanDictPaths(const QString &language, QFileInfo &aff, QFileInfo &dic);
    void unloadLanguage(const LanguageManager::LangId &langId);

private:
    std::list<LangItem>           languages_;
    QStringList                   dictPaths_;
    QSet<LanguageManager::LangId> supportedLangs_;
};

#endif // HUNSPELLCHECKER_H
