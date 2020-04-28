/*
 *
 * opt_pluginswrapper.h - a wrapper for settings pages coming from plugins
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

#ifndef OPT_PLUGINWRAPPER_H
#define OPT_PLUGINWRAPPER_H

#include "optionstab.h"

class PsiOptions;
class QTreeWidgetItem;
class QWidget;
class OAH_PluginOptionsTab;

class OptionsTabPluginWrapper : public OptionsTab {
    Q_OBJECT
public:
    OptionsTabPluginWrapper(OAH_PluginOptionsTab *tab, QObject *parent);
    ~OptionsTabPluginWrapper();

    QWidget *widget() override;
    void     applyOptions() override;
    void     restoreOptions() override;

    QIcon tabIcon() const override;

    QWidget *             w = nullptr;
    OAH_PluginOptionsTab *plugin;
};

#endif // OPT_SHORTCUTS_H
