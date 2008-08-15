/*
 * macspellchecker.cpp
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <Cocoa/Cocoa.h>

#include "macspellchecker.h"

MacSpellChecker::MacSpellChecker()
{
}

MacSpellChecker::~MacSpellChecker()
{
}

bool MacSpellChecker::isCorrect(const QString& word)
{
	NSString* ns_word = [NSString stringWithUTF8String: word.toUtf8().data()];
	NSRange range = {0,0};
	range = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:ns_word startingAt:0];
	return (range.length == 0);
}

QList<QString> MacSpellChecker::suggestions(const QString& word)
{
	QList<QString> s;

	NSString* ns_word = [NSString stringWithUTF8String: word.toUtf8().data()];
	NSArray* ns_suggestions = [[NSSpellChecker sharedSpellChecker] guessesForWord:ns_word];
	for(unsigned int i = 0; i < [ns_suggestions count]; i++) {
		s += QString::fromUtf8([[ns_suggestions objectAtIndex:i] UTF8String]);
	}

	return s;
}

bool MacSpellChecker::add(const QString& word)
{
	return false;
}

bool MacSpellChecker::available() const
{
	return true;
}

bool MacSpellChecker::writable() const
{
	return false;
}
