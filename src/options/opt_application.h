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

	void setHaveAutoUpdater(bool);

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private slots:
	void updatePortLabel();

private:
	QWidget *w;
	bool haveAutoUpdater_;

private slots:
	void doEnableQuitOnClose(int);
};

#endif
