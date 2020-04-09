#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include "options/optionsdlgbase.h"

class OptionsDlg : public OptionsDlgBase {
    Q_OBJECT

public:
    OptionsDlg(PsiCon *, QWidget *parent = nullptr);
};

#endif // OPTIONSDLG_H
