#ifndef OPT_STATUSAUTO_H
#define OPT_STATUSAUTO_H

#include "optionstab.h"
#include "common.h"

class QWidget;

class OptionsTabStatusAuto : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabStatusAuto(QObject *parent);
	~OptionsTabStatusAuto();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

	void setData(PsiCon *, QWidget *parentDialog);

private:
	QWidget *w, *parentWidget;
};

#endif // OPT_STATUSAUTO_H
