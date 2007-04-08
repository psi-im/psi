#ifndef OPT_EVENTS_H
#define OPT_EVENTS_H

#include "optionstab.h"
#include "qradiobutton.h"

class QWidget;
struct Options;

class OptionsTabEvents : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabEvents(QObject *parent);

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private:
	QWidget *w;
	QList<QRadioButton*> list_alerts;
};

#endif
