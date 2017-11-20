#ifndef LANGUAGEMODEL_H
#define LANGUAGEMODEL_H

#include <QAbstractTableModel>
#include <QLocale>

class LanguageModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum ListFlags {
		CountryDiff = 1,
		ScriptDiff  = 2
	};

	struct LangId {
		quint16 language;
		quint16 country;
		quint8  script; // in qt-5.9.2 it's less than 256
		quint8  flags;  // ListFlags
	};

	LanguageModel();

	void setAllLanguages();
	void setLanguages(const QList<QLocale> &list);
	void setLanguages(const QStringList &list);

	// reimplemented
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:

	bool limitedMode;
	QList<LangId> langs;
};

#endif // LANGUAGEMODEL_H
