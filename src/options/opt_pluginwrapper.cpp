/*
 * opt_pluginswrapper.cpp - a wrapper for settings pages coming from plugins
 * Copyright (C) 2020  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "opt_pluginwrapper.h"

#include "optionaccessinghost.h"

#include <QIcon>

//----------------------------------------------------------------------------
// OptionsTabPluginWrapper
//----------------------------------------------------------------------------

/**
 * \brief Constructor of the Options Tab Shortcuts Class
 */
OptionsTabPluginWrapper::OptionsTabPluginWrapper(OAH_PluginOptionsTab *tab, QObject *parent) :
    OptionsTab(parent, tab->id(), tab->parentId(), tab->title(), tab->desc()), plugin(tab)
{
    tab->setCallbacks([this]() { emit dataChanged(); }, [this](bool nd) { emit noDirty(nd); },
                      [this](QWidget *w) { emit connectDataChanged(w); });
}

/**
 * \brief Destructor of the Options Tab Shortcuts Class
 */
OptionsTabPluginWrapper::~OptionsTabPluginWrapper() { }

/**
 * \brief widget, creates the Options Tab Shortcuts Widget
 * \return QWidget*, points to the previously created widget
 */
QWidget *OptionsTabPluginWrapper::widget()
{
    if (w)
        return nullptr;
    w = plugin->widget();
    return w;
}

/**
 * \brief    applyOptions, if options have changed, they will be applied by calling this function
 * \param    opt, unused, totally ignored
 */
void OptionsTabPluginWrapper::applyOptions()
{
    if (!w)
        return;
}

/**
 * \brief    restoreOptions, reads in the currently set options
 */
void OptionsTabPluginWrapper::restoreOptions()
{
    if (!w)
        return;
}

QIcon OptionsTabPluginWrapper::tabIcon() const { return plugin->icon(); }
