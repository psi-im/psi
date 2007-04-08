#ifndef OPT_LOOKFEEL_H
#define OPT_LOOKFEEL_H

#include "optionstab.h"

class QWidget;
struct Options;

class OptionsTabLookFeelToolbars : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabLookFeelToolbars(QObject *parent);

	QWidget *widget();
	void applyOptions(Options *opt);

private slots:
	void setData(PsiCon *, QWidget *);

private:
	PsiCon *psi;
};

#endif
