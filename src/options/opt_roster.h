#ifndef OPT_ROSTER_H
#define OPT_ROSTER_H

#include "optionstab.h"

class QWidget;
class QButtonGroup;

class OptionsTabRoster : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabRoster(QObject *parent);
	~OptionsTabRoster();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private:
	QWidget *w;
};

#endif
