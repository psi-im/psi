/*
 * coloropt.cpp - Psi color options class
 * Copyright (C) 2011  Sergey Ilinykh
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

#include "coloropt.h"

#include "psiapplication.h"
#include "psioptions.h"

#include <QApplication>

#include <cmath>

ColorOpt::ColorOpt() : QObject(nullptr)
{
    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString &)), SLOT(optionChanged(const QString &)));
    connect(PsiOptions::instance(), SIGNAL(destroyed()), SLOT(reset()));
    if (auto *app = qobject_cast<PsiApplication *>(qApp)) {
        connect(app, &PsiApplication::applicationPaletteChanged, this, [this]() {
            // Invalid option colors mean "use the current application palette".
            // Re-emit them when the desktop theme changes so cached users repaint.
            for (auto it = colors.cbegin(); it != colors.cend(); ++it) {
                if (!it.value().color.isValid())
                    emit changed(it.key());
            }
        });
    }

    typedef struct {
        const char         *opt;
        QPalette::ColorRole role;
    } SourceType;
    auto source = std::to_array<SourceType>({ { "contactlist.status.online", QPalette::Text },
                                              { "contactlist.status.offline", QPalette::Text },
                                              { "contactlist.status.away", QPalette::Text },
                                              { "contactlist.status.do-not-disturb", QPalette::Text },
                                              { "contactlist.profile.header-foreground", QPalette::Text },
                                              { "contactlist.profile.header-background", QPalette::Dark },
                                              { "contactlist.grouping.header-foreground", QPalette::Text },
                                              { "contactlist.grouping.header-background", QPalette::Base },
                                              { "contactlist.background", QPalette::Base },
                                              { "contactlist.status-change-animation1", QPalette::Text },
                                              { "contactlist.status-change-animation2", QPalette::Base },
                                              { "contactlist.status-messages", QPalette::Text },
                                              { "muc.role-moderator", QPalette::Text },
                                              { "muc.role-participant", QPalette::Text },
                                              { "muc.role-visitor", QPalette::Text },
                                              { "muc.role-norole", QPalette::Text },
                                              { "tooltip.background", QPalette::ToolTipBase },
                                              { "tooltip.text", QPalette::ToolTipText },
                                              { "messages.received", QPalette::Text },
                                              { "messages.sent", QPalette::Text },
                                              { "messages.informational", QPalette::Text },
                                              { "messages.usertext", QPalette::Text },
                                              { "messages.highlighting", QPalette::Text },
                                              { "messages.link", QPalette::Link },
                                              { "messages.link-visited", QPalette::Link },
                                              { "passive-popup.border", QPalette::Window } });
    for (const auto &item : source) {
        QString opt = QString("options.ui.look.colors.%1").arg(item.opt);
        colors.insert(opt, ColorData(PsiOptions::instance()->getOption(opt).value<QColor>(), item.role));
    }
}

QColor ColorOpt::color(const QString &opt, const QColor &defaultColor) const
{
    ColorData cd = colors.value(opt);
    // qDebug("get option: %s from data %s", qPrintable(opt), qPrintable(cd.color.isValid()? cd.color.name() : "Invalid
    // " + cd.color.name()));
    if (!cd.valid) {
        return PsiOptions::instance()->getOption(opt, defaultColor).value<QColor>();
    }
    if (cd.color.isValid()) {
        return cd.color;
    }
    return QApplication::palette().color(cd.role);
}

QPalette::ColorRole ColorOpt::colorRole(const QString &opt) const { return colors.value(opt).role; }

bool ColorOpt::compatibleColors(const QColor &foreground, const QColor &background)
{
    const int    dR = foreground.red() - background.red();
    const int    dG = foreground.green() - background.green();
    const int    dB = foreground.blue() - background.blue();
    const double dV = std::abs(foreground.value() - background.value());
    const double dC = std::sqrt(0.2126 * dR * dR + 0.7152 * dG * dG + 0.0722 * dB * dB);

    return !((dC < 80. && dV > 100) || (dC < 110. && dV <= 100 && dV > 10) || (dC < 125. && dV <= 10));
}

QColor ColorOpt::ensureContrast(const QColor &foreground, const QColor &background, const QColor &fallback)
{
    if (!foreground.isValid() || !background.isValid() || compatibleColors(foreground, background))
        return foreground;
    if (!fallback.isValid())
        return foreground;

    constexpr int steps = 10;
    for (int step = 1; step <= steps; ++step) {
        const auto blend = [step](int from, int to) { return (from * (steps - step) + to * step) / steps; };
        const QColor candidate(blend(foreground.red(), fallback.red()), blend(foreground.green(), fallback.green()),
                               blend(foreground.blue(), fallback.blue()), foreground.alpha());
        if (compatibleColors(candidate, background))
            return candidate;
    }
    return fallback;
}

QColor ColorOpt::adaptBackground(const QColor &background, const QPalette &palette)
{
    if (!background.isValid())
        return background;

    const QColor base = palette.color(QPalette::Base);
    // A light, explicitly configured header color is usually a leftover from
    // the default light theme. Keep deliberate colors, but do not let such a
    // header turn into a bright stripe when the application palette is dark.
    constexpr int maximumLightnessDifference = 64;
    if (base.lightness() < 128 && background.lightness() - base.lightness() > maximumLightnessDifference)
        return palette.color(QPalette::AlternateBase);

    return background;
}

void ColorOpt::optionChanged(const QString &opt)
{
    if (opt.startsWith(QLatin1String("options.ui.look.colors")) && colors.contains(opt)) {
        colors[opt].color = PsiOptions::instance()->getOption(opt).value<QColor>();
        // qDebug("%s changed to %s", qPrintable(opt), qPrintable(colors[opt].color.isValid()? colors[opt].color.name()
        // : "Invalid " + colors[opt].color.name()));
        emit changed(opt);
    }
}

/**
 * Returns the singleton instance of this class
 * \return Instance of PsiOptions
 */
ColorOpt *ColorOpt::instance()
{
    if (!instance_)
        instance_.reset(new ColorOpt());
    return instance_.get();
}

void ColorOpt::reset() { instance_.reset(nullptr); }

std::unique_ptr<ColorOpt> ColorOpt::instance_;
