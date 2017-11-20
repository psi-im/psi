#include "languagemodel.h"

LanguageModel::LanguageModel() :
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
		LangId id;
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
		const LangId &l = langs[index.row()];
		QLocale loc((QLocale::Language)l.language, (QLocale::Script)l.script, (QLocale::Country)l.country);
		QString name = loc.nativeLanguageName() + " [" + QLocale::languageToString(loc.language()) + "]";
		if (l.flags & ScriptDiff) {
			name += " - " + QLocale::scriptToString(loc.script());
		}
		if (l.flags & CountryDiff) {
			name += " - " + loc.nativeCountryName();
		}
		return name;
	}

	return QVariant();
}
