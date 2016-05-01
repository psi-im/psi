#ifndef ICONFACTORYACCESSOR_H
#define ICONFACTORYACCESSOR_H

class IconFactoryAccessingHost;

class IconFactoryAccessor
{
public:
	virtual ~IconFactoryAccessor() {}

	virtual void setIconFactoryAccessingHost(IconFactoryAccessingHost* host) = 0;

};

Q_DECLARE_INTERFACE(IconFactoryAccessor, "org.psi-im.IconFactoryAccessor/0.1");

#endif // ICONFACTORYACCESSOR_H
