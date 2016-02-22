/*
 * activitycatalog.cpp
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

#include <QString>
#include <QObject>
#include <QCoreApplication>

#include "activity.h"
#include "activitycatalog.h"

ActivityCatalog::Entry::Entry()
{
	type_ = Activity::Unknown;
	specificType_ = Activity::UnknownSpecific;
}

ActivityCatalog::Entry::Entry(Activity::Type type, const QString& value, const QString& text)
{
	type_  = type;
	specificType_ = Activity::UnknownSpecific;
	value_ = value;
	text_  = text;
}

ActivityCatalog::Entry::Entry(Activity::Type type, Activity::SpecificType specificType, const QString& value, const QString& text)
{
	type_ = type;
	specificType_ = specificType;
	value_ = value;
	text_  = text;
}

Activity::Type ActivityCatalog::Entry::type() const
{
	return type_;
}

Activity::SpecificType ActivityCatalog::Entry::specificType() const
{
	return specificType_;
}

const QString& ActivityCatalog::Entry::value() const
{
	return value_;
}

const QString& ActivityCatalog::Entry::text() const
{
	return text_;
}

bool ActivityCatalog::Entry::isNull() const
{
	return type_ == Activity::Unknown && text_.isNull() && value_.isNull();
}

ActivityCatalog::ActivityCatalog() : QObject(QCoreApplication::instance())
{
	entries_ += Entry(Activity::DoingChores, "doing_chores", QObject::tr("Doing Chores"));
	entries_ += Entry(Activity::DoingChores, Activity::BuyingGroceries, "buying_groceries", QObject::tr("Buying Groceries"));
	entries_ += Entry(Activity::DoingChores, Activity::Cleaning, "cleaning", QObject::tr("Cleaning"));
	entries_ += Entry(Activity::DoingChores, Activity::Cooking, "cooking", QObject::tr("Cooking"));
	entries_ += Entry(Activity::DoingChores, Activity::DoingMaintenance, "doing_maintenance", QObject::tr("Doing Maintenance"));
	entries_ += Entry(Activity::DoingChores, Activity::DoingTheDishes, "doing_the_dishes", QObject::tr("Doing The Dishes"));
	entries_ += Entry(Activity::DoingChores, Activity::DoingTheLaundry, "doing_the_laundry", QObject::tr("Doing The Laundry"));
	entries_ += Entry(Activity::DoingChores, Activity::Gardening, "gardening", QObject::tr("Gardening"));
	entries_ += Entry(Activity::DoingChores, Activity::RunningAnErrand, "running_an_errand", QObject::tr("Running An Errand"));
	entries_ += Entry(Activity::DoingChores, Activity::WalkingTheDog, "walking_the_dog", QObject::tr("Walking The Dog"));

	entries_ += Entry(Activity::Drinking, "drinking", QObject::tr("Drinking"));
	entries_ += Entry(Activity::Drinking, Activity::HavingABeer, "having_a_beer", QObject::tr("Having A Beer"));
	entries_ += Entry(Activity::Drinking, Activity::HavingCoffee, "having_coffee", QObject::tr("Having Coffee"));
	entries_ += Entry(Activity::Drinking, Activity::HavingTea, "having_tea", QObject::tr("Having Tea"));

	entries_ += Entry(Activity::Eating, "eating", QObject::tr("Eating"));
	entries_ += Entry(Activity::Eating, Activity::HavingASnack, "having_a_snack", QObject::tr("Having A Snack"));
	entries_ += Entry(Activity::Eating, Activity::HavingBreakfast, "having_breakfast", QObject::tr("Having Breakfast"));
	entries_ += Entry(Activity::Eating, Activity::HavingLunch, "having_lunch", QObject::tr("Having Lunch"));
	entries_ += Entry(Activity::Eating, Activity::HavingDinner, "having_dinner", QObject::tr("Having Dinner"));

	entries_ += Entry(Activity::Exercising, "exercising", QObject::tr("Exercising"));
	entries_ += Entry(Activity::Exercising, Activity::Cycling, "cycling", QObject::tr("Cycling"));
	entries_ += Entry(Activity::Exercising, Activity::Dancing, "dancing", QObject::tr("Dancing"));
	entries_ += Entry(Activity::Exercising, Activity::Hiking, "hiking", QObject::tr("Hiking"));
	entries_ += Entry(Activity::Exercising, Activity::Jogging, "jogging", QObject::tr("Jogging"));
	entries_ += Entry(Activity::Exercising, Activity::PlayingSports, "playing_sports", QObject::tr("Playing Sports"));
	entries_ += Entry(Activity::Exercising, Activity::Running, "running", QObject::tr("Running"));
	entries_ += Entry(Activity::Exercising, Activity::Skiing, "skiing", QObject::tr("Skiing"));
	entries_ += Entry(Activity::Exercising, Activity::Swimming, "swimming", QObject::tr("Swimming"));
	entries_ += Entry(Activity::Exercising, Activity::WorkingOut, "working_out", QObject::tr("Working Out"));

	entries_ += Entry(Activity::Grooming, "grooming", QObject::tr("Grooming"));
	entries_ += Entry(Activity::Grooming, Activity::AtTheSpa, "at_the_spa", QObject::tr("At The Spa"));
	entries_ += Entry(Activity::Grooming, Activity::BrushingTeeth, "brushing_teeth", QObject::tr("Brushing Teeth"));
	entries_ += Entry(Activity::Grooming, Activity::GettingAHaircut, "getting_a_haircut", QObject::tr("Getting A Haircut"));
	entries_ += Entry(Activity::Grooming, Activity::Shaving, "shaving", QObject::tr("Shaving"));
	entries_ += Entry(Activity::Grooming, Activity::TakingABath, "taking_a_bath", QObject::tr("Taking A Bath"));
	entries_ += Entry(Activity::Grooming, Activity::TakingAShower, "taking_a_shower", QObject::tr("Taking A Shower"));

	entries_ += Entry(Activity::HavingAppointment, "having_appointment", QObject::tr("Having Appointment"));

	entries_ += Entry(Activity::Inactive, "inactive", QObject::tr("Inactive"));
	entries_ += Entry(Activity::Inactive, Activity::DayOff, "day_off", QObject::tr("Day Off"));
	entries_ += Entry(Activity::Inactive, Activity::HangingOut, "hanging_out", QObject::tr("Hanging Out"));
	entries_ += Entry(Activity::Inactive, Activity::Hiding, "hiding", QObject::tr("Hiding"));
	entries_ += Entry(Activity::Inactive, Activity::OnVacation, "on_vacation", QObject::tr("On Vacation"));
	entries_ += Entry(Activity::Inactive, Activity::Praying, "praying", QObject::tr("Praying"));
	entries_ += Entry(Activity::Inactive, Activity::ScheduledHoliday, "scheduled_holiday", QObject::tr("Scheduled Holiday"));
	entries_ += Entry(Activity::Inactive, Activity::Sleeping, "sleeping", QObject::tr("Sleeping"));
	entries_ += Entry(Activity::Inactive, Activity::Thinking, "thinking", QObject::tr("Thinking"));

	entries_ += Entry(Activity::Relaxing, "relaxing", QObject::tr("Relaxing"));
	entries_ += Entry(Activity::Relaxing, Activity::Fishing, "fishing", QObject::tr("Fishing"));
	entries_ += Entry(Activity::Relaxing, Activity::Gaming, "gaming", QObject::tr("Gaming"));
	entries_ += Entry(Activity::Relaxing, Activity::GoingOut, "going_out", QObject::tr("Going Out"));
	entries_ += Entry(Activity::Relaxing, Activity::Partying, "partying", QObject::tr("Partying"));
	entries_ += Entry(Activity::Relaxing, Activity::Reading, "reading", QObject::tr("Reading"));
	entries_ += Entry(Activity::Relaxing, Activity::Rehearsing, "rehearsing", QObject::tr("Rehearsing"));
	entries_ += Entry(Activity::Relaxing, Activity::Shopping, "shopping", QObject::tr("Shopping"));
	entries_ += Entry(Activity::Relaxing, Activity::Smoking, "smoking", QObject::tr("Smoking"));
	entries_ += Entry(Activity::Relaxing, Activity::Socializing, "socializing", QObject::tr("Socializing"));
	entries_ += Entry(Activity::Relaxing, Activity::Sunbathing, "sunbathing", QObject::tr("Sunbathing"));
	entries_ += Entry(Activity::Relaxing, Activity::WatchingTv, "watching_tv", QObject::tr("Watching TV"));
	entries_ += Entry(Activity::Relaxing, Activity::WatchingAMovie, "watching_a_movie", QObject::tr("Watching A Movie"));

	entries_ += Entry(Activity::Talking, "talking", QObject::tr("Talking"));
	entries_ += Entry(Activity::Talking, Activity::InRealLife, "in_real_life", QObject::tr("In Real Life"));
	entries_ += Entry(Activity::Talking, Activity::OnThePhone, "on_the_phone", QObject::tr("On The Phone"));
	entries_ += Entry(Activity::Talking, Activity::OnVideoPhone, "on_video_phone", QObject::tr("On Video Phone"));

	entries_ += Entry(Activity::Traveling, "traveling", QObject::tr("Traveling"));
	entries_ += Entry(Activity::Traveling, Activity::Commuting, "commuting", QObject::tr("Commuting"));
	entries_ += Entry(Activity::Traveling, Activity::Cycling, "cycling", QObject::tr("Cycling"));
	entries_ += Entry(Activity::Traveling, Activity::Driving, "driving", QObject::tr("Driving"));
	entries_ += Entry(Activity::Traveling, Activity::InACar, "in_a_car", QObject::tr("In A Car"));
	entries_ += Entry(Activity::Traveling, Activity::OnABus, "on_a_bus", QObject::tr("On A Bus"));
	entries_ += Entry(Activity::Traveling, Activity::OnAPlane, "on_a_plane", QObject::tr("On A Plane"));
	entries_ += Entry(Activity::Traveling, Activity::OnATrain, "on_a_train", QObject::tr("On A Train"));
	entries_ += Entry(Activity::Traveling, Activity::OnATrip, "on_a_trip", QObject::tr("On A Trip"));
	entries_ += Entry(Activity::Traveling, Activity::Walking, "walking", QObject::tr("Walking"));

	entries_ += Entry(Activity::Working, "working", QObject::tr("Working"));
	entries_ += Entry(Activity::Working, Activity::Coding, "coding", QObject::tr("Coding"));
	entries_ += Entry(Activity::Working, Activity::InAMeeting, "in_a_meeting", QObject::tr("In A Meeting"));
	entries_ += Entry(Activity::Working, Activity::Studying, "studying", QObject::tr("Studying"));
	entries_ += Entry(Activity::Working, Activity::Writing, "writing", QObject::tr("Writing"));

	// Undefined specific activity
	entries_ += Entry(Activity::Unknown, Activity::Other, "other", QObject::tr("Other"));
}

ActivityCatalog* ActivityCatalog::instance()
{
	if (!instance_)
		instance_ = new  ActivityCatalog();
	return instance_;
}

ActivityCatalog::Entry ActivityCatalog::findEntryByType(Activity::Type type) const
{
	foreach(Entry e, entries_) {
		if (e.type() == type)
			return e;
	}
	return Entry();
}

ActivityCatalog::Entry ActivityCatalog::findEntryByType(Activity::SpecificType specificType) const
{
	foreach(Entry e, entries_) {
		if (e.specificType() == specificType)
			return e;
	}
	return Entry();
}

ActivityCatalog::Entry ActivityCatalog::findEntryByValue(const QString& value) const
{
	foreach(Entry e, entries_) {
		if (e.value() == value)
			return e;
	}
	return Entry();
}

ActivityCatalog::Entry ActivityCatalog::findEntryByText(const QString& text) const
{
	foreach(Entry e, entries_) {
		if (e.text() == text)
			return e;
	}
	return Entry();
}

const QList<ActivityCatalog::Entry>& ActivityCatalog::entries() const
{
	return entries_;
}

ActivityCatalog* ActivityCatalog::instance_ = 0;
