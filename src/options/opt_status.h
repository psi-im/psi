#ifndef OPT_STATUS_H
#define OPT_STATUS_H

#include "optionstab.h"

class QWidget;
struct Options;

class OptionsTabStatus : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabStatus(QObject *parent);
	~OptionsTabStatus();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

	void setData(PsiCon *, QWidget *parentDialog);
	//bool stretchable() const { return true; }

private slots:
	void selectStatusPreset(int x);
	void newStatusPreset();
	void removeStatusPreset();
	void changeStatusPreset();

private:
	QWidget *w, *parentWidget;
	Options *o;
};

#endif
