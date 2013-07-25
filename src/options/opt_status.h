#ifndef OPT_STATUS_H
#define OPT_STATUS_H

#include <QMap>
#include <QSet>
#include <QStringList>

#include "optionstab.h"
#include "statuspreset.h"

class QWidget;

class OptionsTabStatus : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabStatus(QObject *parent);
	~OptionsTabStatus();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

	void setData(PsiCon *, QWidget *parentDialog);
	//bool stretchable() const { return true; }

private slots:
	void selectStatusPreset(int x);
	void newStatusPreset();
	void removeStatusPreset();
	void changeStatusPreset();

private:
	void setStatusPresetWidgetsEnabled(bool);

	QWidget *w, *parentWidget;
	QMap<QString, StatusPreset> presets;
	QSet<QString> dirtyPresets;
	QStringList deletedPresets;
	QMap<QString, StatusPreset> newPresets;
};

#endif
