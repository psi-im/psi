#ifndef OPT_APPLICATION_H
#define OPT_APPLICATION_H

#include "optionstab.h"

class QWidget;
class QButtonGroup;

class OptionsTabApplication : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabApplication(QObject *parent);
	~OptionsTabApplication();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private:
	QWidget *w;
};

#endif
