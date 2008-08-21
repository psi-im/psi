/*
 * psiplugin.h - Psi plugin interface
 * Copyright (C) 2006-2008  Kevin Smith, Maciej Niedzielski
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

class QWidget;

#include <QtCore>


/**
 * \brief An abstract class for implementing a plugin
 */
class PsiPlugin
{
public:
	virtual ~PsiPlugin() {}

	/**
	 * \brief Plugin Name
	 * The full name of the plugin.
	 * \return Plugin name
	 */
	virtual QString name() const = 0;

	/**
	 * \brief Short name for the plugin
	 * This is the short name of the plugin, used for options structures. 
	 * It must consist of only alphanumerics (no spaces or punctuation).
	 * \return Short plugin name
	*/
	virtual QString shortName() const = 0;
	
	/**
	 * \brief Plugin version
	 * Free-form string of the plugin version. Human readable
	 * \return Plugin version string
	 */
	virtual QString version() const = 0;

	/**
	 * \brief Plugin options widget
	 * This method is called by the Psi options system to retrieve
	 * a widget containing the options for this plugin.
	 * This will then be embedded in the options dialog, so this
	 * should be considered when designing the widget. Should return
	 * NULL when there are no user-configurable options. The calling method
	 * is responsible for deleting the options.
	 *
	 * TODO: make sure this is really deleted, etc
	 */
	virtual QWidget* options() const = 0;

	/**
	 * \brief Enable plugin
	 * \return true if plugin was successfully enabled
	 */

	virtual bool enable() = 0;

	/**
	 * \brief Disable plugin
	 * \return true if plugin was successfully disabled
	 */
	virtual bool disable() = 0;

};

Q_DECLARE_INTERFACE(PsiPlugin, "org.psi-im.PsiPlugin/0.3");

#endif
