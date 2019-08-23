#ifndef PLUGINACCESSOR_H
#define PLUGINACCESSOR_H

class PluginAccessingHost;

class PluginAccessor
{
public:
    virtual ~PluginAccessor() {}

    virtual void setPluginAccessingHost(PluginAccessingHost *host) = 0;
};

Q_DECLARE_INTERFACE(PluginAccessor, "org.psi-im.PluginAccessor/0.1");

#endif // PLUGINACCESSOR_H
