/*
 * spellchecker.h
 *
 * Copyright (C) 2006  Remko Troncon
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

#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include "languagemanager.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

class SpellChecker : public QObject {
public:
    static SpellChecker   *instance();
    virtual bool           available() const;
    virtual bool           writable() const;
    virtual QList<QString> suggestions(const QString &);
    virtual bool           isCorrect(const QString &);
    virtual bool           add(const QString &);

    virtual void                          setActiveLanguages(const QSet<LanguageManager::LangId> &) { }
    virtual QSet<LanguageManager::LangId> getAllLanguages() const { return QSet<LanguageManager::LangId>(); }

protected:
    SpellChecker();
    virtual ~SpellChecker();

private:
    static SpellChecker *instance_;
};

#endif // SPELLCHECKER_H
