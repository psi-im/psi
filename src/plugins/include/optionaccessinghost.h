#ifndef OPTIONACCESSINGHOST_H
#define OPTIONACCESSINGHOST_H

#include <QDomElement>
#include <QVariant>

class OptionAccessingHost
{
public:
	virtual ~OptionAccessingHost() {}

	virtual void setPluginOption(const QString& option, const QVariant& value) = 0;
	virtual QVariant getPluginOption(const QString& option) = 0;

	virtual void setGlobalOption(const QString& option, const QVariant& value) = 0;
	virtual QVariant getGlobalOption(const QString& option) = 0;

};

Q_DECLARE_INTERFACE(OptionAccessingHost, "org.psi-im.OptionAccessingHost/0.1");

#endif
