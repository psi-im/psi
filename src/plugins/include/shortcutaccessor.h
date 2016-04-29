#ifndef SHORTCUTACCESSOR_H
#define SHORTCUTACCESSOR_H

class ShortcutAccessingHost;

class ShortcutAccessor
{
public:
	virtual ~ShortcutAccessor() {}

	virtual void setShortcutAccessingHost(ShortcutAccessingHost* host) = 0;
	virtual void setShortcuts() = 0;

};

Q_DECLARE_INTERFACE(ShortcutAccessor, "org.psi-im.ShortcutAccessor/0.1");

#endif
