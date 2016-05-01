#ifndef SHORTCUTACCESSINGHOST_H
#define SHORTCUTACCESSINGHOST_H

class QObject;
class QKeySequence;

class ShortcutAccessingHost
{
public:
	virtual ~ShortcutAccessingHost() {}

	virtual void connectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot) = 0;
	virtual void disconnectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot) = 0;
	virtual void requestNewShortcut(QObject *receiver, const char* slot) = 0;

};

Q_DECLARE_INTERFACE(ShortcutAccessingHost, "org.psi-im.ShortcutAccessingHost/0.1");

#endif
