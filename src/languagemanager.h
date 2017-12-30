#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QAbstractTableModel>
#include <QLocale>

class LanguageManager
{
public:
    struct LangId {
        quint16 language = QLocale::AnyLanguage;
        quint16 country = QLocale::AnyCountry;
        quint8  script = QLocale::AnyScript; // in qt-5.9.2 it's less than 256
        quint8  flags = 0;  // ListFlags
    };

    static LangId fromString(const QString &langDesc);
    static QString toString(const LangId &id);
    static LangId bestUiMatch(const QList<LangId> &avail);
    static QString languageName(const LangId &id);
    static QString countryName(const LangId &id);
};

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

class LanguageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum ListFlags {
        CountryDiff = 1,
        ScriptDiff  = 2
    };

    LanguageModel(QObject *parent = Q_NULLPTR);

    void setAllLanguages();
    void setLanguages(const QList<QLocale> &list);
    void setLanguages(const QStringList &list);

    // reimplemented
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:

    bool limitedMode;
    QList<LanguageManager::LangId> langs;
};

#endif // LANGUAGEMANAGER_H
