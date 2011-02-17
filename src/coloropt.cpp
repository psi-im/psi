/*
 * coloropt.cpp - Psi color options class
 * Copyright (C) 2011 Rion
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QApplication>

#include "coloropt.h"
#include "psioptions.h"

ColorOpt::ColorOpt()
	: QObject(0)
{
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(PsiOptions::instance(), SIGNAL(destroyed()), SLOT(reset()));

	typedef struct {const char *opt; QPalette::ColorRole role;} SourceType;
	SourceType source[] = {
		{"contactlist.status.online", QPalette::WindowText},
		{"contactlist.status.offline", QPalette::WindowText},
		{"contactlist.status.away", QPalette::WindowText},
		{"contactlist.status.do-not-disturb", QPalette::WindowText},
		{"contactlist.profile.header-foreground", QPalette::WindowText},
		{"contactlist.profile.header-background", QPalette::Window},
		{"contactlist.grouping.header-foreground", QPalette::WindowText},
		{"contactlist.grouping.header-background", QPalette::Window},
		{"contactlist.background", QPalette::Base},
		{"contactlist.status-change-animation1", QPalette::WindowText},
		{"contactlist.status-change-animation2", QPalette::WindowText},
		{"contactlist.status-messages", QPalette::WindowText},
		{"messages.received", QPalette::WindowText},
		{"messages.sent", QPalette::WindowText},
		{"messages.informational", QPalette::WindowText}
	};
	for (unsigned int i = 0; i < sizeof(source) / sizeof(SourceType); i++) {
		QString opt = QString("options.ui.look.colors.%1").arg(source[i].opt);
		colors.insert(opt, ColorData(PsiOptions::instance()->getOption(opt).value<QColor>(), source[i].role));
	}
}

QColor ColorOpt::color(const QString &opt, const QColor &defaultColor) const
{
	ColorData cd = colors.value(opt);
	if (!cd.valid) {
		return PsiOptions::instance()->getOption(opt, defaultColor).value<QColor>();
	}
	if (cd.color.isValid()) {
		return cd.color;
	}
	return QApplication::palette().color(cd.role);
}

QPalette::ColorRole ColorOpt::colorRole(const QString &opt) const
{
	return colors.value(opt).role;
}

void ColorOpt::optionChanged(const QString &opt)
{
	if (opt.startsWith("options.ui.look.colors") && colors.contains(opt)) {
		colors[opt].color = PsiOptions::instance()->getOption(opt).value<QColor>();
	}
}

/**
 * Returns the singleton instance of this class
 * \return Instance of PsiOptions
 */
ColorOpt* ColorOpt::instance()
{
	if ( !instance_ )
		instance_.reset(new ColorOpt());
	return instance_.data();
}

void ColorOpt::reset()
{
	instance_.reset(0);
}

QScopedPointer<ColorOpt> ColorOpt::instance_;
