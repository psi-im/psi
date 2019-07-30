#ifndef OPT_ROSTER_MAIN_H
#define OPT_ROSTER_MAIN_H

#include "optionstab.h"

class QButtonGroup;
class QWidget;

class OptionsTabRosterMain : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabRosterMain(QObject *parent);
    ~OptionsTabRosterMain();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private:
    QWidget *w;
};

#endif
