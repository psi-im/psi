/*
 * activiy.h
 * Copyright (C) 2008 Armando Jagucki
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

#ifndef ACTIVITY_H
#define ACTIVITY_H

#include <QString>
#include <QMetaType>

#define PEP_ACTIVITY_TN "activity"
#define PEP_ACTIVITY_NS "http://jabber.org/protocol/activity"

class QDomElement;
class QDomDocument;

class Activity
{
public:
	enum Type {
		Unknown,
		DoingChores, Drinking, Eating, Exercising, Grooming,
		HavingAppointment, Inactive, Relaxing, Talking, Traveling,
		Working
	};

	enum SpecificType {
		UnknownSpecific,
		// under 'doing_chores'
		BuyingGroceries, Cleaning, Cooking, DoingMaintenance,
		DoingTheDishes, DoingTheLaundry, Gardening, RunningAnErrand,
		WalkingTheDog,
		// under 'drinking'
		HavingABeer, HavingCoffee, HavingTea,
		// under 'eating'
		HavingASnack, HavingBreakfast, HavingLunch, HavingDinner,
		// under 'excercising'
		Cycling, Dancing, Hiking, Jogging, PlayingSports, Running, Skiing,
		Swimming, WorkingOut,
		// under 'grooming'
		AtTheSpa, BrushingTeeth, GettingAHaircut, Shaving, TakingABath,
		TakingAShower,
		// under 'having_appointment'
		// under 'inactive'
		DayOff, HangingOut, Hiding, OnVacation, Praying, ScheduledHoliday, Sleeping, Thinking,
		// under 'relaxing'
		Fishing, Gaming, GoingOut, Partying, Reading, Rehearsing, Shopping, Smoking,
		Socializing, Sunbathing, WatchingTv, WatchingAMovie,
		// under 'talking'
		InRealLife, OnThePhone, OnVideoPhone,
		// under 'traveling'
		Commuting, /* Cycling (already included), */ Driving, InACar,
		OnABus, OnAPlane, OnATrain, OnATrip, Walking,
		// under 'working'
		Coding, InAMeeting, Studying, Writing,
		// undefined specific activity
		Other
	};

	Activity();
	Activity(Type, SpecificType, const QString& = QString());
	Activity(const QDomElement&);

	Type type() const;
	QString typeText() const;
	QString typeValue() const;
	QString specificTypeValue() const;
	SpecificType specificType() const;
	QString specificTypeText() const;
	const QString& text() const;
	bool isNull() const;

	QDomElement toXml(QDomDocument&);

protected:
	void fromXml(const QDomElement&);

private:
	Type type_;
	SpecificType specificType_;
	QString text_;
};

Q_DECLARE_METATYPE(Activity)

#endif
