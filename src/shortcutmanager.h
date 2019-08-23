#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QList>
#include <QObject>

class PsiOptions;
class QKeySequence;
class QString;

class ShortcutManager : public QObject
{
public:
    static ShortcutManager* instance();
    static void connect(const QString& path, QObject *parent, const char* slot);
    QKeySequence shortcut(const QString& name);
    QList<QKeySequence> shortcuts(const QString& name);

    // utils
    static QList<QKeySequence> readShortcutsFromOptions(const QString& name, const PsiOptions* options);

private:
    ShortcutManager();
    static ShortcutManager* instance_;
};

#endif // SHORTCUTMANAGER_H
