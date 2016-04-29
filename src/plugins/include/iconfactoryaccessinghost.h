#ifndef ICONFACTORYACCESSINGHOST_H
#define ICONFACTORYACCESSINGHOST_H

class QByteArray;
class QString;

class IconFactoryAccessingHost
{
public:
	virtual ~IconFactoryAccessingHost() {}

	virtual void addIcon(const QString& name, const QByteArray& icon) = 0;
	virtual QIcon getIcon(const QString& name) = 0;
};

Q_DECLARE_INTERFACE(IconFactoryAccessingHost, "org.psi-im.IconFactoryAccessingHost/0.1");
#endif // ICONFACTORYACCESSINGHOST_H
