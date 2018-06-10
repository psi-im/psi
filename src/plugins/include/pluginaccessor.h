#ifndef PSI_PLUGINACCESSOR_H
#define PSI_PLUGINACCESSOR_H

class PluginAccessingHost;

class PluginAccessor
{
public:
    virtual ~PluginAccessor() {}

    virtual void setPluginAccessingHost(PluginAccessingHost *host) = 0;
};

Q_DECLARE_INTERFACE(PluginAccessor, "org.psi-im.PluginAccessor/0.1");

#endif //PSI_PLUGINACCESSOR_H
