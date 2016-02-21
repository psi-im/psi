#ifndef OPT_STATUSGENERAL_H
#define OPT_STATUSGENERAL_H

#include <QMap>
#include <QListWidgetItem>
#include <QMenu>

#include "optionstab.h"
#include "statuspreset.h"

class QWidget;

class OptionsTabStatusGeneral : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabStatusGeneral(QObject *parent);
	~OptionsTabStatusGeneral();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

	void setData(PsiCon *, QWidget *parentDialog);
	bool stretchable() const { return true; }

signals:
	void enableDlgCommonWidgets(bool);

private slots:
	void currentItemChanged(QListWidgetItem * current, QListWidgetItem * previous );
	void newStatusPreset();
	void deleteStatusPreset();
	void statusMenusIndexChanged( int index );
	void showMenuForPreset(const QPoint &);
	void editStatusPreset();
	void statusPresetAccepted();
	void statusPresetRejected();
	void presetDoubleClicked(const QModelIndex &);

private:
	void loadStatusPreset();
	void saveStatusPreset();
	void switchPresetMode(bool toEdit);
	void cleanupSelectedPresetGroup();

	QWidget *w, *parentWidget;
	QMap<QString, StatusPreset> presets;
	QMenu *spContextMenu;
};

#endif // OPT_STATUSGENERAL_H
