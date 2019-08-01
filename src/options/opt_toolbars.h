#ifndef OPT_TOOLBARS_H
#define OPT_TOOLBARS_H

#include "optionstab.h"

class IconButton;
class PsiCon;
class PsiToolBar;
class QAction;
class QListWidget;
class QListWidgetItem;
class QTreeWidgetItem;

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

private:
    void updateArrows();

    QWidget *w = nullptr;
    QWidget *parent = nullptr;
    PsiCon *psi = nullptr;
    bool noDirty;
    IconButton *pb_apply = nullptr;

    class Private;
    Private *p;
};

#endif // OPT_TOOLBARS_H
