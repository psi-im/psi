#ifndef OPT_PLUGINS_H
#define OPT_PLUGNS_H

#include "optionstab.h"

class QWidget;

class OptionsTabPlugins : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabPlugins(QObject *parent);
	~OptionsTabPlugins();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private:
	QWidget *w;
	QWidget *pluginWidget;

private slots:
	void listPlugins();
	void pluginSelected(int index);
	void loadToggled(int state);
};

#endif
