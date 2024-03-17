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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "enchantchecker.h"

#include "enchant++.h"

#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QtDebug>

static enchant::Broker *broker;

EnchantChecker::EnchantChecker()
{
#ifdef HAVE_ENCHANT2
    broker = new enchant::Broker();
#else
    broker = enchant::Broker::instance();
#endif
    if (broker) {
        broker->list_dicts(enchantDictDescribeFn, static_cast<void *>(this));
        QTimer::singleShot(1000, [=]() { this->setActiveLanguages(getAllLanguages()); });
    }
}

EnchantChecker::~EnchantChecker()
{
    clearSpellers();
#ifdef HAVE_ENCHANT2
    delete broker;
#endif
}

bool EnchantChecker::isCorrect(const QString &word)
{
    if (spellers_.isEmpty())
        return true;

    for (enchant::Dict *speller : spellers_) {
        if (speller->check(word.toUtf8().constData()))
            return true;
    }
    return false;
}

QList<QString> EnchantChecker::suggestions(const QString &word)
{
    QList<QString> words;

    for (enchant::Dict *speller : spellers_) {
        std::vector<std::string> out_suggestions;
        speller->suggest(word.toUtf8().constData(), out_suggestions);
        std::vector<std::string>::iterator aE = out_suggestions.end();
        for (std::vector<std::string>::iterator aI = out_suggestions.begin(); aI != aE; ++aI) {
            words += QString::fromUtf8(aI->c_str());
        }
    }
    return words;
}

bool EnchantChecker::add(const QString &word)
{
    bool result = false;
    if (!spellers_.isEmpty()) {
        QString trimmed_word = word.trimmed();
        if (!word.isEmpty()) {
#ifdef HAVE_ENCHANT2
            spellers_.first()->add(word.toUtf8().constData());
#else
            spellers_.first()->add_to_pwl(word.toUtf8().constData());
#endif
            result = true;
        }
    }
    return result;
}

bool EnchantChecker::available() const { return (spellers_.isEmpty() != true); }

bool EnchantChecker::writable() const { return false; }

QSet<LanguageManager::LangId> EnchantChecker::getAllLanguages() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    return allLanguages_.keys().toSet();
#else
    return QSet<LanguageManager::LangId>(allLanguages_.keyBegin(), allLanguages_.keyEnd());
#endif
}

void EnchantChecker::setActiveLanguages(const QSet<LanguageManager::LangId> &langs)
{
    clearSpellers();

    for (auto const &lang : langs) {
        auto it = allLanguages_.constFind(lang);
        if (it == allLanguages_.constEnd())
            continue;

        try {
            spellers_ << broker->request_dict(it.value().toStdString());
        } catch (enchant::Exception &e) {
            qWarning() << QString("Enchant error: %1").arg(e.what());
        }
    }
}

void EnchantChecker::clearSpellers()
{
    qDeleteAll(spellers_);
    spellers_.clear();
}

void EnchantChecker::enchantDictDescribeFn(const char *const lang_tag, const char *const provider_name,
                                           const char *const provider_desc, const char *const provider_file,
                                           void *user_data)
{
    Q_UNUSED(provider_name);
    Q_UNUSED(provider_desc);
    Q_UNUSED(provider_file);
    EnchantChecker *enchantChecker = static_cast<EnchantChecker *>(user_data);

    QString lang(QString::fromLatin1(lang_tag));
    auto    id = LanguageManager::fromString(lang);
    if (!id.language) {
        id = LanguageManager::fromString(lang.section(QLatin1Char('_'), 0, 0));
    }
    if (id.language) {
        if (!enchantChecker->allLanguages_.contains(id)) {
            enchantChecker->allLanguages_.insert(id, lang);
        }
    }
}
