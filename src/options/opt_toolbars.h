#ifndef OPT_TOOLBARDLG_H
#define OPT_TOOLBARDLG_H

#include "optionstab.h"

class PsiCon;
class PsiToolBar;
class QAction;
class IconButton;
class QListWidget;
class QTreeWidgetItem;
class QListWidgetItem;
class OptionsTabToolbars : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabToolbars(QObject* parent);
	~OptionsTabToolbars();

	bool stretchable() const {return true;};
	
	//void setCurrentToolbar(PsiToolBar *);
	QWidget* widget();
	void applyOptions();
	void restoreOptions();

private slots:
	void setData(PsiCon*, QWidget*);
	void toolbarAdd();
	void toolbarDelete();
	void addToolbarAction(QListWidget *, QString name, int toolbarId);
	void addToolbarAction(QListWidget *, const QAction *action, QString name);
	void toolbarSelectionChanged(int);
	void rebuildToolbarKeys();
	void toolbarNameChanged();
	void toolbarActionUp();
	void toolbarActionDown();
	void toolbarAddAction();
	void toolbarRemoveAction();
	void toolbarDataChanged();
	QString actionName(const QAction *a);
	void toolbarPosition();
	void selAct_selectionChanged(QListWidgetItem *);
	void avaAct_selectionChanged(QTreeWidgetItem *);

	void toolbarPositionApply();

signals:
	void dataChanged();

private:
	void updateArrows();

	QWidget *w;
	QWidget *parent;
	PsiCon *psi;
	bool noDirty;
	IconButton *pb_apply;

	class Private;
	Private *p;
};

#endif
