#ifndef OPT_APPLICATION_H
#define OPT_APPLICATION_H

#include "optionstab.h"

class QWidget;
struct Options;
class QButtonGroup;

class OptionsTabApplication : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabApplication(QObject *parent);
	~OptionsTabApplication();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private:
	QWidget *w;
};

#endif
