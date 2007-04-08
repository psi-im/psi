#ifndef OPT_CHAT_H
#define OPT_CHAT_H

#include "optionstab.h"

class QWidget;
struct Options;
class QButtonGroup;

class OptionsTabChat : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabChat(QObject *parent);
	~OptionsTabChat();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private:
	QWidget *w;
	QButtonGroup *bg_defAct, *bg_delChats;
};

#endif
