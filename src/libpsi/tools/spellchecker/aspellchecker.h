/*
 * aspellchecker.h
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

#ifndef ASPELLCHECKER_H
#define ASPELLCHECKER_H

#include "spellchecker.h"

#include <QList>
#include <QString>

struct AspellConfig;
struct AspellSpeller;

class ASpellChecker : public SpellChecker {
public:
    ASpellChecker();
    ~ASpellChecker();
    virtual QList<QString> suggestions(const QString &);
    virtual bool           isCorrect(const QString &);
    virtual bool           add(const QString &);
    virtual bool           available() const;
    virtual bool           writable() const;

    virtual void                          setActiveLanguages(const QSet<LanguageManager::LangId> &langs);
    virtual QSet<LanguageManager::LangId> getAllLanguages() const;

private:
    void clearSpellers();

private:
    AspellConfig *config_;

    typedef QList<AspellSpeller *> ASpellers;
    ASpellers                      spellers_;
};

#endif // ASPELLCHECKER_H
