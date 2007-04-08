#include <QAction>

#include "psioptions.h"
#include "shortcutmanager.h"


/**
 * \brief The Construtor of the Shortcutmanager
 */
ShortcutManager::ShortcutManager() 
{
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
	QString type = variant.typeName();
	if (type == "QVariantList")
		variant = variant.toList().first();
	//qDebug() << "shortcut: " << name << variant.value<QKeySequence>().toString();
	return variant.value<QKeySequence>();
}

/**
 * \brief get the QVariantList associated with the keyname "name"
 * \param the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage
 * \return List of sequences
 */
QList<QKeySequence> ShortcutManager::shortcuts(const QString& name)
{
	QList<QKeySequence> list;
	QVariant variant = PsiOptions::instance()->getOption(QString("options.shortcuts.%1").arg(name));
	QString type = variant.typeName();
	if (type == "QVariantList") {
		foreach(QVariant variant, variant.toList()) {
			list += variant.value<QKeySequence>();
		}
	}
	else {
		list += variant.value<QKeySequence>();
	}
	return list;
}


/*
 * \brief this function connects the Key or Keys associated with the keyname "path" with the slot "slot"
 *        of the Widget "parent"
 * \param path, the shortcut name e.g. "misc.sendmessage" which is in the options xml then
 *        mirrored as options.shortcuts.misc.sendmessage 
 * \param parent, the widget to which the new QAction should be connected to
 * \param slot, the SLOT() of the parent which should be triggerd if the KeySequence is activated
 */
void ShortcutManager::connect(const QString& path, QWidget *parent, const char* slot) 
{
	if(parent == NULL || slot == NULL)
		return;

	foreach(QKeySequence sequence, ShortcutManager::instance()->shortcuts(path)) {
		QAction* act = new QAction(parent);
		act->setShortcut(sequence);
		parent->addAction(act);
		parent->connect(act, SIGNAL(activated()), slot);
	}
}
