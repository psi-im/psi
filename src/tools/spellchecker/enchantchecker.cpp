/*
 * enchantchecker.cpp
 *
 * Copyright (C) 2009  Caolán McNamara
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

#include "enchant++.h"
#include "enchantchecker.h"

EnchantChecker::EnchantChecker() : speller_(NULL)
{
	if (enchant::Broker *instance = enchant::Broker::instance())
	{
		std::string lang("en_US");
        	if (instance->dict_exists(QTextCodec::locale()))
			lang = QTextCodec::locale();
		try {
			speller_ = enchant::Broker::instance()->request_dict(lang);
		} catch (enchant::Exception &e) {
			qWarning() << QString("Enchant error: %1").arg(e.what());
		}
	}
}

EnchantChecker::~EnchantChecker()
{
	delete speller_;
	speller_ = NULL;
}

bool EnchantChecker::isCorrect(const QString& word)
{
	if(speller_) {
		return speller_->check(word.toUtf8().constData());
	}
	return true;
}

QList<QString> EnchantChecker::suggestions(const QString& word)
{
	QList<QString> words;
	if (speller_) {
		std::vector<std::string> out_suggestions;
		speller_->suggest(word.toUtf8().constData(), out_suggestions);
		std::vector<std::string>::iterator aE = out_suggestions.end();
		for (std::vector<std::string>::iterator aI = out_suggestions.begin(); aI != aI; ++aI) {
			words += QString::fromUtf8(aI->c_str());
		}
	}
	return words;
}

bool EnchantChecker::add(const QString& word)
{
	bool result = false;
	if (speller_) {
		QString trimmed_word = word.trimmed();
		if(!word.isEmpty()) {
			speller_->add_to_pwl(word.toUtf8().constData());
			result = true;
		}
	}
	return result;
}

bool EnchantChecker::available() const
{
	return (speller_ != NULL);
}

bool EnchantChecker::writable() const
{
	return false;
}
