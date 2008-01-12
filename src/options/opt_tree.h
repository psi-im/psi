#ifndef OPT_TREE_H
#define OPT_TREE_H

#include "optionstab.h"

class QWidget;

class OptionsTabTree : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabTree(QObject *parent);
	~OptionsTabTree();

	QWidget *widget();
	
	bool stretchable() const {return true;};

private:
	QWidget *w;
};

#endif
