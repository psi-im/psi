#include "languagemanager.h"

#include <QRegularExpression>
#include <QSet>

LanguageManager::LangId LanguageManager::fromString(const QString &langDesc)
{
    QLocale loc(langDesc);
    LangId  id;
    if (loc == QLocale::c()) {
        return id; // It's default initialized to any lang, any country, any script. consider as a error
    }
    int cnt     = langDesc.count(QRegularExpression("[_-]"));
    id.language = loc.language();
    if (cnt) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        id.country = loc.country(); // supposing if there are two components then it's lways country and not script
#else
        id.country = loc.territory();
#endif
        if (cnt > 1) { // lang_script_country
            id.script = loc.script();
        }
    }
    return id;
}

// returns [lang][-script][-country]
QString LanguageManager::toString(const LanguageManager::LangId &id)
{
    QLocale     loc((QLocale::Language)id.language, (QLocale::Script)id.script, (QLocale::Country)id.country);
    QStringList ret;
    QStringList langCountry = loc.name().split('_');
    if (id.language) {
        ret.append(langCountry[0]); // language
    }
    if (id.script) {
        QStringList items = loc.bcp47Name().split(QRegularExpression("[.@]"))[0].split('-');
        if (items.count() == 3) { // we have script
            ret.append(items[1]);
        }
    }
    if (id.country) {
        ret.append(langCountry[1]);
    }
    return ret.join('-');
}

/**
 * @brief LanguageManager::bestUiMatch
 *
 *  Lookups the best match from available locales against each next
 *  locale from QLocale::uiLanguages.
 *  For example available is comprised of en_ANY, en_US, ru_ANY (depends on LangId fields),
 *  and uiLanguages has ru_RU then "ru_ANY" will be selected and returned.
 *  If uiLanguages locale is en_US for the example above, then en_US
 *  will be selected with language and country in LangId.
 *
 *  Another case is when available have something like en_US, ru_RU, ru_UA but
 *  uiLanguages has just "ru" then system locale will be checked for country.
 *  In case of Russia, ru_RU will be selected for Belarus nothing will selected.
 *
 *  Exmaples:
 *  available    |  ui          | selected          |
 *  -------------------------------------------------
 *  en_ANY en_US | en_US        | en_US
 *  en_ANY       | en_US        | en_ANY
 *  en_US        | en           | en_US if system is US. nothing otherwise
 *
 *
 * @param avail available languages to select from.
 * @param justOne just one langId in result is enough
 * @return priority sorted languages list. best match comes first
 */
QList<LanguageManager::LangId> LanguageManager::bestUiMatch(const QSet<LanguageManager::LangId> &avail, bool justOne)
{
    QLocale             def; // default locale (or system locale if default is not set). FIXME get from settings
    static QSet<LangId> uiLangs;
    if (uiLangs.isEmpty()) {
        const auto languages = QLocale::system().uiLanguages();
        for (auto const &l : languages) {
            auto id = fromString(l);
            if (id.language) {
                uiLangs.insert(id);
            }
        }
    }
    QList<LangId> ret;
    QList<LangId> toCheck;
    toCheck.reserve(4);
    for (auto uiId : uiLangs) {
        toCheck.clear();
        // check if ui locale looks like system locale and set missed parts
        if (uiId.language == def.language()) { // matches with system. consider country and script from system to be
                                               // preferred if not set in ui
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            auto country = def.country();
#else
            auto country = def.territory();
#endif
            if (!uiId.country && (!uiId.script || uiId.script == def.script())) {
                uiId.country = country;
            }
            if (!uiId.script && (!uiId.country || uiId.country == country)) {
                uiId.script = def.script();
            }
        }
        // now when everything is filled we can fallback from example 3 to 2 if it was system.
        // of course it's still possible to have just language in ui.

        // next things are simple.
        // first try uiId match as is and add to result if it's within available.
        // then check if we can remove script or country and try again.
        // repeat till we have just language
        toCheck.append(uiId);

        auto copyId = uiId;
        // try to check with any script
        if (uiId.script != QLocale::AnyScript) {
            uiId.script = QLocale::AnyScript;
            toCheck.append(uiId);
            uiId = copyId;
        }
        // try to check with any country
        if (uiId.country != QLocale::AnyCountry) {
            uiId.country = QLocale::AnyCountry;
            toCheck.append(uiId);
            uiId = copyId;
        }
        // try to check any script and any country
        if (uiId.script != QLocale::AnyScript && uiId.country != QLocale::AnyCountry) {
            uiId.script  = QLocale::AnyScript;
            uiId.country = QLocale::AnyCountry;
            toCheck.append(uiId);
        }

        for (auto const &id : toCheck) {
            if (avail.contains(id)) {
                ret.append(id);
                if (justOne) {
                    return ret;
                }
            }
        }
    }
    LangId defLangId;
    if (avail.contains(defLangId)) {
        ret.append(defLangId);
    }
    return ret;
}

QString LanguageManager::bestUiMatch(QHash<QString, QString> langToText)
{
    QHash<LangId, QString> langs;
    for (auto l = langToText.constBegin(); l != langToText.constEnd(); ++l) {
        langs.insert(LanguageManager::fromString(l.key()),
                     l.value()); // FIXME: all unknown languages will be converted to C/default locale
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    auto preferred = LanguageManager::bestUiMatch(langs.keys().toSet(), true);
#else
    auto preferred = LanguageManager::bestUiMatch(QSet<LangId>(langs.keyBegin(), langs.keyEnd()), true);
#endif
    if (preferred.count()) {
        return langs.value(preferred.first());
    }
    return QString();
}

QString LanguageManager::languageName(const LanguageManager::LangId &id)
{
    bool needCountry = true;
    if (id.language == QLocale::AnyLanguage) {
        return QObject::tr("Any Language");
    }
    QLocale loc((QLocale::Language)id.language, (QLocale::Script)id.script, (QLocale::Country)id.country);

    QString name;
    if (loc.language() == QLocale::English || loc.language() == QLocale::Spanish) {
        // english and espanol use country in language name
        if (id.country) {
            needCountry = false;
        } else {
            name = loc.language() == QLocale::English ? QStringLiteral("English") : QStringLiteral("Español");
        }
    }

    if (name.isEmpty()) {
        name = loc.nativeLanguageName();
    }
    if (name.isEmpty()) {
        name = QLocale::languageToString(loc.language());
    } else if (loc.script() != QLocale::LatinScript
               && loc.script()
                   != QLocale().script()) { // if not latin and not deafuls, then probaby it's somethingunreadable
        name += (" [" + QLocale::languageToString(loc.language()) + "]");
    }
    if (id.script) {
        name += " - " + QLocale::scriptToString(loc.script());
    }
    if (needCountry && id.country) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        name += " - " + loc.nativeCountryName();
#else
        name += " - " + loc.nativeTerritoryName();
#endif
    }
    return name;
}

QString LanguageManager::countryName(const LanguageManager::LangId &id)
{
    QLocale loc((QLocale::Language)id.language, (QLocale::Script)id.script, (QLocale::Country)id.country);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QString ret = loc.nativeCountryName();
#else
    QString ret = loc.nativeTerritoryName();
#endif
    if (loc.language() != QLocale().language() && loc.script() != QLocale::LatinScript) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        ret += " (" + loc.countryToString(loc.country()) + ")";
#else
        ret += " (" + loc.territoryToString(loc.territory()) + ")";
#endif
    }
    return ret;
}

QSet<LanguageManager::LangId> LanguageManager::deserializeLanguageSet(const QString &str)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList langs = str.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
#else
    QStringList langs = str.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
#endif
    QSet<LangId> ret;
    for (auto const &l : langs) {
        auto id = fromString(l);
        if (id.language) {
            ret.insert(id);
        }
    }
    return ret;
}

QString LanguageManager::serializeLanguageSet(const QSet<LanguageManager::LangId> &langs)
{
    QStringList ret;
    for (auto const &l : langs) {
        ret.append(toString(l));
    }
    return ret.join(' ');
}
