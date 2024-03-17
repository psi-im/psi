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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "aspellchecker.h"

#include "aspell.h"

#include <QCoreApplication>
#include <QDir>
#include <QtDebug>

ASpellChecker::ASpellChecker() : config_(new_aspell_config())
{
    aspell_config_replace(config_, "encoding", "utf-8");
#ifdef Q_OS_WIN
    aspell_config_replace(config_, "conf-dir", QDir::homePath().toLocal8Bit().data());
    aspell_config_replace(config_, "data-dir",
                          QString("%1/aspell").arg(QCoreApplication::applicationDirPath()).toLocal8Bit().data());
    aspell_config_replace(config_, "dict-dir",
                          QString("%1/aspell").arg(QCoreApplication::applicationDirPath()).toLocal8Bit().data());
#endif
    setActiveLanguages(getAllLanguages());
}

ASpellChecker::~ASpellChecker()
{
    if (config_) {
        delete_aspell_config(config_);
        config_ = NULL;
    }

    clearSpellers();
}

bool ASpellChecker::isCorrect(const QString &word)
{
    if (spellers_.isEmpty())
        return true;

    for (AspellSpeller *speller : spellers_) {
        if (aspell_speller_check(speller, word.toUtf8().constData(), -1) != 0)
            return true;
    }
    return false;
}

QList<QString> ASpellChecker::suggestions(const QString &word)
{
    QList<QString> words;

    for (AspellSpeller *speller : spellers_) {
        const AspellWordList    *list     = aspell_speller_suggest(speller, word.toUtf8(), -1);
        AspellStringEnumeration *elements = aspell_word_list_elements(list);
        const char              *c_word;
        while ((c_word = aspell_string_enumeration_next(elements)) != NULL) {
            QString suggestion = QString::fromUtf8(c_word);
            if (suggestion.size() > 2)
                words.append(suggestion);
        }
        delete_aspell_string_enumeration(elements);
    }
    return words;
}

bool ASpellChecker::add(const QString &word)
{
    bool result = false;
    if (config_ && !spellers_.empty()) {
        QString trimmed_word = word.trimmed();
        if (!word.isEmpty()) {
            aspell_speller_add_to_personal(spellers_.first(), trimmed_word.toUtf8(), trimmed_word.toUtf8().length());
            aspell_speller_save_all_word_lists(spellers_.first());
            result = true;
        }
    }
    return result;
}

bool ASpellChecker::available() const { return !spellers_.isEmpty(); }

bool ASpellChecker::writable() const { return false; }

QSet<LanguageManager::LangId> ASpellChecker::getAllLanguages() const
{
    QSet<LanguageManager::LangId> langs;

    AspellDictInfoList *dict_info_list = get_aspell_dict_info_list(config_);

    if (!aspell_dict_info_list_empty(dict_info_list)) {
        AspellDictInfoEnumeration *dict_info_enum = aspell_dict_info_list_elements(dict_info_list);

        while (!aspell_dict_info_enumeration_at_end(dict_info_enum)) {
            const AspellDictInfo *dict_info = aspell_dict_info_enumeration_next(dict_info_enum);
            auto                  id        = LanguageManager::fromString(QString::fromLatin1(dict_info->code));
            if (id.language) {
                langs.insert(id);
            }
        }

        delete_aspell_dict_info_enumeration(dict_info_enum);
    }

    return langs;
}

void ASpellChecker::setActiveLanguages(const QSet<LanguageManager::LangId> &langs)
{
    clearSpellers();

    for (auto const &lang : langs) {
        AspellConfig *conf = aspell_config_clone(config_);
        aspell_config_replace(
            conf, "lang",
            LanguageManager::toString(lang).replace(QLatin1Char('-'), QLatin1Char('_')).toUtf8().constData());
        AspellCanHaveError *ret = new_aspell_speller(conf);
        if (aspell_error_number(ret) == 0) {
            spellers_.append(to_aspell_speller(ret));
        } else {
            qDebug() << QString("Aspell error: %1").arg(aspell_error_message(ret));
        }
        delete_aspell_config(conf);
    }
}

void ASpellChecker::clearSpellers()
{
    for (AspellSpeller *speller : spellers_)
        delete_aspell_speller(speller);

    spellers_.clear();
}
