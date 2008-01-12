#ifndef OPT_ADVANCED_H
#define OPT_ADVANCED_H

#include "optionstab.h"

class QWidget;

class OptionsTabAdvanced : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabAdvanced(QObject *parent);
	~OptionsTabAdvanced();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private:
	QWidget *w;
};

#endif
