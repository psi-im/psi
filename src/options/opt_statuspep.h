#ifndef OPT_STAUSPEP_H
#define OPT_STAUSPEP_H

#include "optionstab.h"
#include "common.h"
#include "psicon.h"

class QWidget;

class OptionsTabStatusPep : public OptionsTab
{
	Q_OBJECT

public:

	OptionsTabStatusPep(QObject *parent);
	~OptionsTabStatusPep();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();
	void setData(PsiCon *psi, QWidget *);

protected slots:
	void controllerSelected(bool);

private:
	QWidget *w_;
	PsiCon *psi_;
	QStringList blackList_;
	QString tuneFilters_;
	bool controllersChanged_;
};


#endif // OPT_STAUSPEP_H
