#ifndef GUITEST_H
#define GUITEST_H

#include <QList>
#include <QString>

class GUITest
{
public:
	virtual ~GUITest() { }
	virtual bool run() = 0;
	virtual QString name() = 0;
};

#endif
