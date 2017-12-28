#include "languagemanager.h"

LanguageModel::LanguageModel(QObject *parent) :
    QAbstractTableModel(parent),
    limitedMode(false)
{

}

void LanguageModel::setAllLanguages()
{
	QList<QLocale> allLocales = QLocale::matchingLocales(
	            QLocale::AnyLanguage,
	            QLocale::AnyScript,
	            QLocale::AnyCountry);
	setLanguages(allLocales);
}

void LanguageModel::setLanguages(const QList<QLocale> &list)
{
	limitedMode = true;
	langs.clear();
	if (list.isEmpty()) {
		return;
	}

	struct Named {
		QLocale locale;
		QString name;
	};

	QMap<QString,QLocale> sortedList;
	for (const QLocale &l : list) {
		sortedList.insert(QLocale::languageToString(l.language())+
		                  QLocale::scriptToString(l.script())+
		                  QLocale::countryToString(l.country()), l);
	}

	int firstDupInd = 0, curInd = 0;
	quint8 flags = 0;
	for (const auto &n : sortedList) {
        LanguageManager::LangId id;
		id.country  = static_cast<quint16>(n.country());
		id.language = static_cast<quint16>(n.language());
		id.script   = static_cast<quint16>(n.script());
		langs.append(id);
		if (id.language != langs[firstDupInd].language) {
			for (int i = firstDupInd; i < curInd; i++) {
				langs[i].flags = flags;
			}
			flags = 0;
			firstDupInd = curInd;
		} else {
			if (id.country != langs[firstDupInd].country) {
				flags |= CountryDiff;
			}
			if (id.script != langs[firstDupInd].script) {
				flags |= ScriptDiff;
			}
		}
		curInd++;
	}
	for (int i = firstDupInd; i < langs.count(); i++) {
		langs[i].flags = flags;
	}
}

void LanguageModel::setLanguages(const QStringList &list)
{
	limitedMode = true;
	QList<QLocale> locs;
	for (const QString &s : list) {
		QLocale l(s);
		if (l != QLocale::c()) {
			locs.append(l);
		}
	}
	setLanguages(locs);
}

int LanguageModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid())
		return langs.size();
	return 0;
}

int LanguageModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid())
		return 1; // or two if we need anything else? flags, checkboxes, whatever?
	return 0;
}

QVariant LanguageModel::data(const QModelIndex &index, int role) const
{
	if (index.column() != 0 || index.row() > langs.size()) {
		return QVariant();
	}
	if (role == Qt::DisplayRole) {
        const LanguageManager::LangId &l = langs[index.row()];
        return LanguageManager::languageName(l);
	}

	return QVariant();
}



LanguageManager::LangId LanguageManager::fromString(const QString &langDesc)
{
	QLocale loc(langDesc);
	LangId id;
	if (loc == QLocale::c()) {
		return id; // It's default initialized to any lang, any country, any script. consider as a error
	}
	int cnt = langDesc.count(QRegExp("[_-]"));
	id.language = loc.language();
	if (cnt > 1) {
		id.country = loc.country();
		if (cnt > 2) { // lang_script_country
			id.script = loc.script();
		}
	}
	return id;
}

// returns [lang][-script][-country]
QString LanguageManager::toString(const LanguageManager::LangId &id)
{
	QLocale loc((QLocale::Language)id.language, (QLocale::Script)id.script, (QLocale::Country)id.country);
	QStringList ret;
	QStringList langCountry = loc.name().split('_');
	if (id.language) {
		ret.append(langCountry[0]); // language
	}
	if (id.script) {
		QStringList items = loc.bcp47Name().split(QRegExp("[.@]"))[0].split('-');
		if (items.count() == 3) { // we have script
			ret.append(items[1]);
		}
	}
	if (id.country) {
		ret.append(langCountry[1]);
	}
	return ret.join('-');
}

LanguageManager::LangId LanguageManager::bestUiMatch(const QList<LanguageManager::LangId> &avail)
{
	QLocale def; // default locale (or system locale if default is not set). FIXME get from settings
	LangId preferred;
	for (auto const &aId: avail) {
		if (def.language() != aId.language ||
		        (aId.script != QLocale::AnyScript && aId.script != def.script()) ||
		        (aId.country != QLocale::AnyCountry && aId.country != def.country()))
		{ // we don't care about different languages/scripts/countries
			continue;
		}
		// now if ui lang in the list of available we can check if the current available has better match by country and script
		if (preferred.language == QLocale::AnyLanguage) { // if preferred is not initialized
			preferred = aId;
			continue;
		}
		if (preferred.script == QLocale::AnyScript && aId.script != QLocale::AnyScript) {
			preferred = aId;
			continue;
		}
		if (preferred.country == QLocale::AnyCountry && aId.country != QLocale::AnyCountry) {
			preferred = aId;
		}
		if (preferred.script && preferred.country) {
			break;
		}
	}
	return preferred;
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
            name = loc.language() == QLocale::English? QStringLiteral("English") : QStringLiteral("Español");
        }
    }

    if (name.isEmpty()) {
        name = loc.nativeLanguageName();
    }
	if (name.isEmpty()) {
		name = QLocale::languageToString(loc.language());
	}
	else if (loc.script() != QLocale::LatinScript && loc.script() != QLocale().script()) { // if not latin and not deafuls, then probaby it's somethingunreadable
		name += (" [" + QLocale::languageToString(loc.language()) + "]");
	}
	if (id.script) {
		name += " - " + QLocale::scriptToString(loc.script());
	}
    if (needCountry && id.country) {
		name += " - " + loc.nativeCountryName();
	}
	return name;
}
