#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include "options/optionsdlgbase.h"

class OAH_PluginOptionsTab;

class OptionsDlg : public OptionsDlgBase {
    Q_OBJECT

public:
    OptionsDlg(PsiCon *, QWidget *parent = nullptr);
    void addPluginWrapperTab(OAH_PluginOptionsTab *tab);
};

#endif // OPTIONSDLG_H
