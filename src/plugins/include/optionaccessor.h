#ifndef OPTIONACCESSOR_H
#define OPTIONACCESSOR_H

#include <QDomElement>
//#include <QString>

class OptionAccessingHost;

class OptionAccessor
{
public:
	virtual ~OptionAccessor() {}

	virtual void setOptionAccessingHost(OptionAccessingHost* host) = 0;

	virtual void optionChanged(const QString& option) = 0;
};

Q_DECLARE_INTERFACE(OptionAccessor, "org.psi-im.OptionAccessor/0.1");

#endif
