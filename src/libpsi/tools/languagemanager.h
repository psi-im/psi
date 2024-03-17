#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QAbstractTableModel>
#include <QLocale>

class LanguageManager {
public:
    struct LangId {
        quint16 language = QLocale::AnyLanguage;
        quint16 country  = QLocale::AnyCountry;
        quint8  script   = QLocale::AnyScript; // in qt-5.9.2 it's less than 256
    };

    static LangId                         fromString(const QString &langDesc);
    static QString                        toString(const LangId &id);
    static QList<LanguageManager::LangId> bestUiMatch(const QSet<LanguageManager::LangId> &avail, bool justOne = false);
    static QString                        bestUiMatch(QHash<QString, QString> langToText);
    static QString                        languageName(const LangId &id);
    static QString                        countryName(const LangId &id);
    static QSet<LangId>                   deserializeLanguageSet(const QString &);
    static QString                        serializeLanguageSet(const QSet<LanguageManager::LangId> &langs);
};

inline uint qHash(const LanguageManager::LangId &t) { return qHash(t.language) ^ qHash(t.country) ^ qHash(t.script); }

// weird sorting operator
inline bool operator<(const LanguageManager::LangId &a, const LanguageManager::LangId &b)
{
    if (a.script == b.script) {
        return ((a.language << 16) + a.country) < ((b.language << 16) + b.country);
    }
    return a.script < b.script;
}

inline bool operator==(const LanguageManager::LangId &a, const LanguageManager::LangId &b)
{
    return a.language == b.language && a.country == b.country && a.script == b.script;
}

Q_DECLARE_METATYPE(LanguageManager::LangId)

#endif // LANGUAGEMANAGER_H
