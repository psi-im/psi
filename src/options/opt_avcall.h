#ifndef OPT_AVCALL_H
#define OPT_AVCALL_H

#include "optionstab.h"

class QButtonGroup;
class QWidget;

class OptionsTabAvCall : public OptionsTab {
    Q_OBJECT
public:
    OptionsTabAvCall(QObject *parent);
    ~OptionsTabAvCall();

    QWidget *widget();
    void     applyOptions();
    void     restoreOptions();

private:
    QWidget *w;
};

#endif // OPT_AVCALL_H
