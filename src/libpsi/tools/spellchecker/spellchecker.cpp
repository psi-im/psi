/*
 * spellchecker.cpp
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

#include "spellchecker.h"

#if defined(Q_OS_MAC) && !defined(HAVE_HUNSPELL)
#include "macspellchecker.h"
#elif defined(HAVE_ENCHANT)
#include "enchantchecker.h"
#elif defined(HAVE_ASPELL)
#include "aspellchecker.h"
#elif defined(HAVE_HUNSPELL)
#include "hunspellchecker.h"
#endif

#include <QCoreApplication>

SpellChecker *SpellChecker::instance()
{
    if (!instance_) {
#if defined(Q_OS_MAC) && !defined(HAVE_HUNSPELL)
        instance_ = new MacSpellChecker();
#elif defined(HAVE_ENCHANT)
        instance_ = new EnchantChecker();
#elif defined(HAVE_ASPELL)
        instance_ = new ASpellChecker();
#elif defined(HAVE_HUNSPELL)
        instance_ = new HunspellChecker();
#else
        instance_ = new SpellChecker();
#endif
    }
    return instance_;
}

SpellChecker::SpellChecker() : QObject(QCoreApplication::instance()) { }

SpellChecker::~SpellChecker() { }

bool SpellChecker::available() const { return false; }

bool SpellChecker::writable() const { return true; }

bool SpellChecker::isCorrect(const QString &) { return true; }

QList<QString> SpellChecker::suggestions(const QString &) { return QList<QString>(); }

bool SpellChecker::add(const QString &) { return false; }

SpellChecker *SpellChecker::instance_ = nullptr;
