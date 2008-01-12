#ifndef OPT_EVENTS_H
#define OPT_EVENTS_H

#include "optionstab.h"
#include "qradiobutton.h"

class QWidget;

class OptionsTabEvents : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabEvents(QObject *parent);

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private:
	QWidget *w;
	QList<QRadioButton*> list_alerts;
};

#endif
