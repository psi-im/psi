/*
 * alertable.cpp - simplify alert icon plumbing
 * Copyright (C) 2006  Michail Pishchagin
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

#include "alertable.h"

#include <QIcon>

#include "alerticon.h"

/**
 * Class to simplify alert icon plumbing. You'll have to re-implement
 * alertFrameUpdated() in your subclass.
 */
Alertable::Alertable(QObject* parent)
	: QObject(parent)
{
	alert_ = 0;
}

/**
 * Destroys alert icon along with itself;
 */
Alertable::~Alertable()
{
	setAlert(0);
}

/**
 * Returns true if alert is set, and false otherwise.
 */
bool Alertable::alerting() const
{
	return alert_ != 0;
}

/**
 * Returns current animation frame of alert, or null QIcon, if 
 * there is no alert.
 */
QIcon Alertable::currentAlertFrame() const
{
	if (!alert_)
		return QIcon();

	return alert_->impix().pixmap();
}

/**
 * Creates new AlertIcon based on provided \param icon. If 0 is passed,
 * alert is cleared.
 */
void Alertable::setAlert(const PsiIcon* icon)
{
	if (alert_) {
		alert_->stop();
		delete alert_;
		alert_ = 0;
	}

	if (icon) {
		alert_ = new AlertIcon(icon);
		alert_->activated(false);

		// connect(alert_, SIGNAL(pixmapChanged()), SLOT(alertFrameUpdated()));
		// alertFrameUpdated();
	}
}
