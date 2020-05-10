#ifndef OPT_TABINPUT_H
#define OPT_TABINPUT_H

#include "languagemanager.h"
#include "optionstab.h"
#include "psicon.h"

class QTreeWidgetItem;
class QWidget;

class OptionsTabInput : public OptionsTab {
    Q_OBJECT
public:
    OptionsTabInput(QObject *parent);
    ~OptionsTabInput();

    QWidget *widget();
    void     applyOptions();
    void     restoreOptions();
    void     setData(PsiCon *psi, QWidget *);
    bool     stretchable() const;

private:
    QStringList obtainDefaultLang() const;
    void        fillList();
    void        setChecked();
    void        updateDictLists();
    bool        isTreeViewEmpty();

private slots:
    void itemToggled(bool toggled);
    void itemChanged(QTreeWidgetItem *item, int column);

private:
    QWidget *                     w_;
    PsiCon *                      psi_;
    QSet<LanguageManager::LangId> availableDicts_;
    QSet<LanguageManager::LangId> loadedDicts_;
    QSet<LanguageManager::LangId> defaultLangs_;
};

#endif // OPT_TABINPUT_H
