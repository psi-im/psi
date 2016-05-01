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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
	// Priorities allows plugins to make processing more ordered. For example
	// some plugins may require process stanzas as early as possible, others
	// may want to do some work at the end. So here here are 5 levels of
	// priority which plugin may choose from. If plugin is not aware about
	// priority then Normal will be choosed for it.
	// While writing plugins its desirable to think twice before choosing
	// Lowest or Highest priority, since your plugin may be not the only which
	// need it. Think about for example stopspam plugin which is known to be
	// highest prioroty blocker/processor. Are you writing stopspam? If not
	// choose High if you want something more then Normal.
	enum Priority
	{
		PriorityLowest	= 0, // always in the end. last loaded Lowest plugin moves other Lowest to Low side
		PriorityLow		= 1,
		PriorityNormal	= 2, // default
		PriorityHigh	= 3,
		PriorityHighest	= 4, // always in the start. last loaded Highest plugin moves others to High side
	};

	virtual ~PsiPlugin() {}

	virtual Priority priority() { return PriorityNormal; }

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
	virtual QWidget* options() = 0;

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

	virtual void applyOptions() = 0;
	virtual void restoreOptions() = 0;

	virtual QPixmap icon() const = 0;
};

Q_DECLARE_INTERFACE(PsiPlugin, "org.psi-im.PsiPlugin/0.4");

#endif
