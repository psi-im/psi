/*
 * psiplugin.h - Psi plugin interface
 * Copyright (C) 2006-2006  Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef PSIPLUGIN_H
#define PSIPLUGIN_H

#include <QObject>
#include <QtCore>

class PsiAccount;
class QDomElement;
class QString;
class QPluginLoader;

namespace XMPP {
	  class Jid;
}


/**
 * \brief An abstract class for implementing a plugin
 */
class PsiPlugin : public QObject
{
	Q_OBJECT
public:
	/**
	 * \brief Plugin Name
	 * The full name of the plugin.
	 * \return Plugin name
	 */
	virtual QString name() const = 0;
	/** \brief Short name for the plugin
	 *	This is the short name of the plugin, used for options structures. 
	 * It must consist of only alphanumerics (no spaces or punctuation).
	 * \return Short plugin name
	**/
	virtual QString shortName() const = 0;
	
	/**
	 * \brief Plugin version
	 * Free-form string of the plugin version. Human readable
	 * \return Plugin version string
	 */
	virtual QString version() const = 0; 
	
	virtual void message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay) 
	{Q_UNUSED(account);Q_UNUSED(message);Q_UNUSED(fromJid);Q_UNUSED(fromDisplay);}

	/**
	 * \brief Plugin options widget
	 * This method is called by the Psi options system to retrieve
	 * a widget containing the options for this plugin.
	 * This will then be embedded in the options dialog, so this
	 * should be considered when designing the widget. Should return
	 * NULL when there are no user-configurable options.
	 */
	virtual QWidget* options() {return NULL;} 
	

	virtual bool processEvent( const PsiAccount* account, QDomNode &event ) {Q_UNUSED(account);Q_UNUSED(event);return true;}

	/**
	 * Convenience method for plugins, allowing them to convert a QDomElement to a QString
	 */
	static QString toString(const QDomNode& xml)
	{
		QTextStream stream;
		stream.setString(new QString(""));
		xml.save(stream, 0);
		return QString(*stream.string());
	}
	
signals:
	/**
	 *	\brief Signals that the plugin wants to send a stanza.
	 * 
	 * \param account The account name, as used by the plugin interface.
	 * \param stanza The stanza to be sent.
	 */
	//void sendStanza( const PsiAccount* account, const QDomElement& stanza);

	/**
	 *	\brief Signals that the plugin wants to send a stanza.
	 * 
	 * \param account The account name, as used by the plugin interface.
	 * \param stanza The stanza to be sent.
	 */
	void sendStanza( const PsiAccount* account, const QString& stanza);
	
	/**
	 * \brief Requests an item in the Psi menu for the plugin
	 * 
	 * \param label The text to be inserted in the menu
	 */
	void registerMainMenu( const QString& label);

	/**
	 * \brief Sets an option (local to the plugin)
	 * The options will be automatically prefixed by the plugin manager, so
	 * there is no need to uniquely name the options. In the same way as the
	 * main options system, a hierachy is available by dot-delimiting the 
	 * levels ( e.g. "emoticons.show"). Use this and not setGlobalOption
	 * in almost every case.
	 * \param  option Option to set
	 * \param value New option value
	 */
	void setPluginOption( const QString& option, const QVariant& value);
	
	/**
	 * \brief Gets an option (local to the plugin)
	 * The options will be automatically prefixed by the plugin manager, so
	 * there is no need to uniquely name the options. In the same way as the
	 * main options system, a hierachy is available by dot-delimiting the 
	 * levels ( e.g. "emoticons.show"). Use this and not getGlobalOption
	 * in almost every case.
	 * \param  option Option to set
	 * \param value Return value
	 */
	void getPluginOption( const QString& option, QVariant& value);
	
	/**
	 * \brief Sets a global option (not local to the plugin)
	 * The options will be passed unaltered by the plugin manager, so
	 * the options are presented as they are stored in the main option
	 * system. Use setPluginOption instead of this in almost every case.
	 * \param  option Option to set
	 * \param value New option value
	 */
	void setGlobalOption( const QString& option, const QVariant& value);
	
	/**
	 * \brief Gets a global option (not local to the plugin)
	 * The options will be passed unaltered by the plugin manager, so
	 * the options are presented as they are stored in the main option
	 * system. Use getPluginOption instead of this in almost every case.
	 * \param  option Option to set
	 * \param value Return value
	 */
	void getGlobalOption( const QString& option, QVariant& value);
	
//protected:

	

};

Q_DECLARE_INTERFACE(PsiPlugin, "org.psi-im.plugin/0.2.1");

#endif
