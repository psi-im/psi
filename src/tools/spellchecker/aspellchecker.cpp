/*
 * aspellchecker.cpp
 *
 * Copyright (C) 2006  Remko Troncon
 * Thanks to Ephraim.
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

#include <QDir>
#include <QCoreApplication>
#include <QtDebug>

#include "aspell.h"
#include "aspellchecker.h"

ASpellChecker::ASpellChecker()
{
	config_ = NULL;
	speller_ = NULL;
	config_ = new_aspell_config();
	aspell_config_replace(config_, "encoding", "utf-8");
#ifdef Q_WS_WIN
	aspell_config_replace(config_, "conf-dir", QDir::homeDirPath());
	aspell_config_replace(config_, "data-dir", QString("%1/aspell/data").arg(QCoreApplication::applicationDirPath()));
	aspell_config_replace(config_, "dict-dir", QString("%1/aspell/dict").arg(QCoreApplication::applicationDirPath()));
#endif
	AspellCanHaveError* ret = new_aspell_speller(config_);
	if (aspell_error_number(ret) == 0) {
		speller_ = to_aspell_speller(ret);
	}
	else {
		qWarning() << QString("Aspell error: %1").arg(aspell_error_message(ret));
	}
}

ASpellChecker::~ASpellChecker()
{
	if(config_) {
		delete_aspell_config(config_);
		config_ = NULL;
	}

	if(speller_) {
		delete_aspell_speller(speller_);
		speller_ = NULL;
	}
}

bool ASpellChecker::isCorrect(const QString& word)
{
	if(speller_) {
		int correct = aspell_speller_check(speller_, word.toUtf8().constData(), -1);
		return (correct != 0);
	}
	return true;
}

QList<QString> ASpellChecker::suggestions(const QString& word)
{
	QList<QString> words;
	if (speller_) {
		const AspellWordList* list = aspell_speller_suggest(speller_, word.toUtf8(), -1); 
		AspellStringEnumeration* elements = aspell_word_list_elements(list);
		const char *c_word;
		while ((c_word = aspell_string_enumeration_next(elements)) != NULL) {
			words += QString::fromUtf8(c_word);
		}
		delete_aspell_string_enumeration(elements);
	}
	return words;
}

bool ASpellChecker::add(const QString& word)
{
	bool result = false;
	if (config_ && speller_) {
		QString trimmed_word = word.trimmed();
		if(!word.isEmpty()) {
			aspell_speller_add_to_personal(speller_, trimmed_word.toUtf8(), trimmed_word.toUtf8().length());
			aspell_speller_save_all_word_lists(speller_);
			result = true;
		}
	}
	return result;
}

bool ASpellChecker::available() const
{
	return (speller_ != NULL);
}

bool ASpellChecker::writable() const
{
	return false;
}
