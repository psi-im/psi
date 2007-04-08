#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QKeySequence>
#include <QString>
#include <QList>

class ShortcutManager
{
public:
	static ShortcutManager* instance();
	static void connect(const QString& path, QWidget *parent, const char* slot);
	QKeySequence shortcut(const QString& name);
	QList<QKeySequence> shortcuts(const QString& name);

private:
	ShortcutManager();
	static ShortcutManager* instance_;
};

#endif
