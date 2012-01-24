/*
 * nullplugin.h - null-op Psi plugin
 * Copyright (C) 2006  Kevin Smith
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

#include "psiplugin.h"

class NullPlugin : public QObject, public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options() const;
	virtual bool enable();
	virtual bool disable();
};

Q_EXPORT_PLUGIN(NullPlugin);

QString NullPlugin::name() const
{
	return "Null Plugin";
}

QString NullPlugin::shortName() const
{
	return "null";
}

QString NullPlugin::version() const
{
	return "0.1";
}

QWidget* NullPlugin::options() const
{
	return 0;
}

bool NullPlugin::enable()
{
	return true;
}

bool NullPlugin::disable()
{
	return true;
}

#include "nullplugin.moc"
