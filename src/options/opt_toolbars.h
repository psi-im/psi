#ifndef OPT_TOOLBARDLG_H
#define OPT_TOOLBARDLG_H

#include "optionstab.h"

class PsiCon;
class PsiToolBar;
struct Options;
class QAction;
class IconButton;
class Q3ListView;
class Q3ListViewItem;

class OptionsTabToolbars : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabToolbars(QObject* parent);
	~OptionsTabToolbars();

	//void setCurrentToolbar(PsiToolBar *);
	QWidget* widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options* opt);

private slots:
	void setData(PsiCon*, QWidget*);
	void toolbarAdd();
	void toolbarDelete();
	void addToolbarAction(Q3ListView *, QString name, int toolbarId);
	void addToolbarAction(Q3ListView *, const QAction *action, QString name);
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
	void selAct_selectionChanged(Q3ListViewItem *);
	void avaAct_selectionChanged(Q3ListViewItem *);

	void doApply();
	void toolbarPositionApply();

	void rebuildToolbarList();

signals:
	void dataChanged();

private:
	void updateArrows();

	QWidget *w;
	QWidget *parent;
	Options *opt;
	PsiCon *psi;
	bool noDirty;
	IconButton *pb_apply;

	class Private;
	Private *p;
};

#endif
