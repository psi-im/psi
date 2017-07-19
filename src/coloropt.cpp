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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
		{"contactlist.status.online", QPalette::Text},
		{"contactlist.status.offline", QPalette::Text},
		{"contactlist.status.away", QPalette::Text},
		{"contactlist.status.do-not-disturb", QPalette::Text},
		{"contactlist.profile.header-foreground", QPalette::Text},
		{"contactlist.profile.header-background", QPalette::Dark},
		{"contactlist.grouping.header-foreground", QPalette::Text},
		{"contactlist.grouping.header-background", QPalette::Base},
		{"contactlist.background", QPalette::Base},
		{"contactlist.status-change-animation1", QPalette::Text},
		{"contactlist.status-change-animation2", QPalette::Base},
		{"contactlist.status-messages", QPalette::Text},
		{"tooltip.background", QPalette::Base},
		{"tooltip.text", QPalette::Text},
		{"messages.received", QPalette::Text},
		{"messages.sent", QPalette::Text},
		{"messages.informational", QPalette::Text},
		{"messages.usertext", QPalette::Text},
		{"messages.highlighting", QPalette::Text},
		{"passive-popup.border", QPalette::Window}
	};
	for (unsigned int i = 0; i < sizeof(source) / sizeof(SourceType); i++) {
		QString opt = QString("options.ui.look.colors.%1").arg(source[i].opt);
		colors.insert(opt, ColorData(PsiOptions::instance()->getOption(opt).value<QColor>(), source[i].role));
	}
}

QColor ColorOpt::color(const QString &opt, const QColor &defaultColor) const
{
	ColorData cd = colors.value(opt);
	//qDebug("get option: %s from data %s", qPrintable(opt), qPrintable(cd.color.isValid()? cd.color.name() : "Invalid " + cd.color.name()));
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
	if (opt.startsWith(QLatin1String("options.ui.look.colors")) && colors.contains(opt)) {
		colors[opt].color = PsiOptions::instance()->getOption(opt).value<QColor>();
		//qDebug("%s changed to %s", qPrintable(opt), qPrintable(colors[opt].color.isValid()? colors[opt].color.name() : "Invalid " + colors[opt].color.name()));
		emit changed();
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
