#include <QAction>
#include <QCoreApplication>

#include "psioptions.h"
#include "shortcutmanager.h"
#include "globalshortcut/globalshortcutmanager.h"


/**
 * \brief The Construtor of the Shortcutmanager
 */
ShortcutManager::ShortcutManager() : QObject(QCoreApplication::instance())
{
	// Make sure that there is at least one shortcut for sending messages
	if (shortcuts("chat.send").isEmpty()) {
		qWarning("Restoring chat.send shortcut");
		QVariantList vl;
		vl << qVariantFromValue(QKeySequence(Qt::Key_Enter)) << qVariantFromValue(QKeySequence(Qt::Key_Return));
		PsiOptions::instance()->setOption("options.shortcuts.chat.send",vl);
	}
}

/**
 * "The one and only instance" of the ShortcutManager
 */
ShortcutManager* ShortcutManager::instance_ = NULL;

/**
 * \brief The Instantiator of the Shortcutmanager
 */
ShortcutManager* ShortcutManager::instance() 
{
	if(!instance_)
		instance_ = new ShortcutManager();
	return instance_;
}

/**
 * \brief get the QKeySequence associated with the keyname "name"
 * \param the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage
 * \return QKeySequence, the Keysequence associated with the keyname, or
 *   the first key sequence if there are multiple
 */
QKeySequence ShortcutManager::shortcut(const QString& name) 
{
	QVariant variant = PsiOptions::instance()->getOption(QString("options.shortcuts.%1").arg(name));
	QList<QKeySequence> list = shortcuts(name);
	if (!list.isEmpty())
		return list.first();
	return variant.value<QKeySequence>();
}

/**
 * Users tend to get really confused when they see 'Return' in menu
 * shortcuts. 'Enter' is much more common, so we'd better use it.
 *
 * Examples:
 * 'Enter' should be preferred over 'Return' and 'Ctrl+Enter' should be
 * preferred over 'Ctrl+Return'.
 */
static bool shortcutManagerKeySequenceLessThan(const QKeySequence& k1, const QKeySequence& k2)
{
	bool e1 = k1.toString(QKeySequence::PortableText).contains("Enter");
	bool e2 = k2.toString(QKeySequence::PortableText).contains("Enter");
	return e1 && !e2;
}

/**
 * \brief get the QVariantList associated with the keyname "name"
 * \param the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage
 * \return List of sequences
 */
QList<QKeySequence> ShortcutManager::shortcuts(const QString& name)
{
	return readShortcutsFromOptions(name, PsiOptions::instance());
}
 
/**
 * \brief read the QVariantList associated with the keyname "name" in given PsiOptions
 * \param name, the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage
 * \param options, options instance to read from
 * \return List of sequences
 */
QList<QKeySequence> ShortcutManager::readShortcutsFromOptions(const QString& name, const PsiOptions* options)
{
	QList<QKeySequence> list;
	QVariant variant = options->getOption(QString("options.shortcuts.%1").arg(name));
	QString type = variant.typeName();
	if (type == "QVariantList") {
		foreach(QVariant variant, variant.toList()) {
			QKeySequence k = variant.value<QKeySequence>();
			if (!k.isEmpty() && !list.contains(k))
				list += k;
		}
	}
	else {
		QKeySequence k = variant.value<QKeySequence>();
		if (!k.isEmpty())
			list += k;
	}
	qStableSort(list.begin(), list.end(), shortcutManagerKeySequenceLessThan);
	return list;
}

/**
 * \brief this function connects the Key or Keys associated with the keyname "path" with the slot "slot"
 *        of the Widget "parent"
 * \param path, the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage 
 * \param parent, the widget to which the new QAction should be connected to
 * \param slot, the SLOT() of the parent which should be triggered if the KeySequence is activated
 */
void ShortcutManager::connect(const QString& path, QObject* parent, const char* slot)
{
	if (parent == NULL || slot == NULL)
		return;

	if (!path.startsWith("global.")) {
		QList<QKeySequence> shortcuts = ShortcutManager::instance()->shortcuts(path);

		if (!shortcuts.isEmpty()) {
			bool appWide = path.startsWith("appwide.");
			QAction* act = new QAction(parent);
			act->setShortcuts(shortcuts);
			act->setShortcutContext(appWide ?
			                        Qt::ApplicationShortcut : Qt::WindowShortcut);
			if (parent->isWidgetType())
				((QWidget*) parent)->addAction(act);
			parent->connect(act, SIGNAL(activated()), slot);
		}
	}
	else {
		foreach(QKeySequence sequence, ShortcutManager::instance()->shortcuts(path)) {
			if (!sequence.isEmpty()) {
				GlobalShortcutManager::instance()->connect(sequence, parent, slot);
			}
		}
	}
}
