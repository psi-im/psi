#ifndef PSI_PLUGINACCESSINGHOST_H
#define PSI_PLUGINACCESSINGHOST_H

class PsiPlugin;

class PluginAccessingHost
{
public:
    virtual ~PluginAccessingHost() {}

    virtual QObject* getPlugin(const QString& name) = 0;
};

Q_DECLARE_INTERFACE(PluginAccessingHost, "org.psi-im.PluginAccessingHost/0.1");

#endif //PSI_PLUGINACCESSINGHOST_H
