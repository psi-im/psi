#ifndef OPT_GROUPCHAT_H
#define OPT_GROUPCHAT_H

#include "optionstab.h"

class QListWidgetItem;
class QWidget;

class OptionsTabGroupchat : public OptionsTab {
    Q_OBJECT
public:
    OptionsTabGroupchat(QObject *parent);

    enum coloringType { NONE = 0, HASH = 1, MANUAL = 2 };
    QWidget *widget();
    void     applyOptions();
    void     restoreOptions();

    void setData(PsiCon *, QWidget *);
    bool stretchable() const { return true; }

private slots:
    void addNickColor(QString);
    void addGCHighlight();
    void removeGCHighlight();
    void addGCNickColor();
    void removeGCNickColor();
    void chooseGCNickColor();
    void displayGCNickColor();
    void selectedGCNickColor(QListWidgetItem *item);
    void updateWidgetsState();

private:
    QWidget *w = nullptr, *dlg = nullptr;
};

#endif // OPT_GROUPCHAT_H
