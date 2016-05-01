#ifndef PLUGININFOPROVIDER_H
#define PLUGININFOPROVIDER_H

class QString;

class PluginInfoProvider
{
public:
	virtual ~PluginInfoProvider() {}

	virtual QString pluginInfo() = 0;

};

Q_DECLARE_INTERFACE(PluginInfoProvider, "org.psi-im.PluginInfoProvider/0.1");


#endif // PLUGININFOPROVIDER_H
