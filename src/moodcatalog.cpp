/*
 * moodcatalog.cpp
 * Copyright (C) 2006  Remko Troncon
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

#include <QString>
#include <QObject>
#include <QCoreApplication>

#include "mood.h"
#include "moodcatalog.h"

MoodCatalog::Entry::Entry()
{
	type_ = Mood::Unknown;
}

MoodCatalog::Entry::Entry(Mood::Type type, const QString& value, const QString& text) : type_(type), value_(value), text_(text)
{
}

Mood::Type MoodCatalog::Entry::type() const
{
	return type_;
}

const QString& MoodCatalog::Entry::value() const
{
	return value_;
}

const QString& MoodCatalog::Entry::text() const
{
	return text_;
}

bool MoodCatalog::Entry::isNull() const
{
	return type_ == Mood::Unknown && text_.isNull() && value_.isNull();
}

bool MoodCatalog::Entry::operator<(const MoodCatalog::Entry &m) const
{
	if ( type_ == Mood::Undefined )
		return false;
	if ( m.type_ == Mood::Undefined )
		return true;
	int result = QString::localeAwareCompare ( text_, m.text_ );
	return result<0;
}

MoodCatalog::MoodCatalog() : QObject(QCoreApplication::instance())
{
	entries_ += Entry(Mood::Afraid, "afraid", QObject::tr("Afraid"));
	entries_ += Entry(Mood::Amazed, "amazed", QObject::tr("Amazed"));
	entries_ += Entry(Mood::Angry, "angry", QObject::tr("Angry"));
	entries_ += Entry(Mood::Amorous, "amorous", QObject::tr("Amorous"));
	entries_ += Entry(Mood::Annoyed, "annoyed", QObject::tr("Annoyed"));
	entries_ += Entry(Mood::Anxious, "anxious", QObject::tr("Anxious"));
	entries_ += Entry(Mood::Aroused, "aroused", QObject::tr("Aroused"));
	entries_ += Entry(Mood::Ashamed, "ashamed", QObject::tr("Ashamed"));
	entries_ += Entry(Mood::Bored, "bored", QObject::tr("Bored"));
	entries_ += Entry(Mood::Brave, "brave", QObject::tr("Brave"));
	entries_ += Entry(Mood::Calm, "calm", QObject::tr("Calm"));
	entries_ += Entry(Mood::Cautious, "cautious", QObject::tr("Cautious"));
	entries_ += Entry(Mood::Cold, "cold", QObject::tr("Cold"));
	entries_ += Entry(Mood::Confident, "confident", QObject::tr("Confident"));
	entries_ += Entry(Mood::Confused, "confused", QObject::tr("Confused"));
	entries_ += Entry(Mood::Contemplative, "contemplative", QObject::tr("Contemplative"));
	entries_ += Entry(Mood::Contented, "contented", QObject::tr("Contented"));
	entries_ += Entry(Mood::Cranky, "cranky", QObject::tr("Cranky"));
	entries_ += Entry(Mood::Crazy, "crazy", QObject::tr("Crazy"));
	entries_ += Entry(Mood::Creative, "creative", QObject::tr("Creative"));
	entries_ += Entry(Mood::Curious, "curious", QObject::tr("Curious"));
	entries_ += Entry(Mood::Dejected, "dejected", QObject::tr("Dejected"));
	entries_ += Entry(Mood::Depressed, "depressed", QObject::tr("Depressed"));
	entries_ += Entry(Mood::Disappointed, "disappointed", QObject::tr("Disappointed"));
	entries_ += Entry(Mood::Disgusted, "disgusted", QObject::tr("Disgusted"));
	entries_ += Entry(Mood::Dismayed, "dismayed", QObject::tr("Dismayed"));
	entries_ += Entry(Mood::Distracted, "distracted", QObject::tr("Distracted"));
	entries_ += Entry(Mood::Embarrassed, "embarrassed", QObject::tr("Embarrassed"));
	entries_ += Entry(Mood::Envious, "envious", QObject::tr("Envious"));
	entries_ += Entry(Mood::Excited, "excited", QObject::tr("Excited"));
	entries_ += Entry(Mood::Flirtatious, "flirtatious", QObject::tr("Flirtatious"));
	entries_ += Entry(Mood::Frustrated, "frustrated", QObject::tr("Frustrated"));
	entries_ += Entry(Mood::Grumpy, "grumpy", QObject::tr("Grumpy"));
	entries_ += Entry(Mood::Guilty, "guilty", QObject::tr("Guilty"));
	entries_ += Entry(Mood::Happy, "happy", QObject::tr("Happy"));
	entries_ += Entry(Mood::Hopeful, "hopeful", QObject::tr("Hopeful"));
	entries_ += Entry(Mood::Hot, "hot", QObject::tr("Hot"));
	entries_ += Entry(Mood::Humbled, "humbled", QObject::tr("Humbled"));
	entries_ += Entry(Mood::Humiliated, "humiliated", QObject::tr("Humiliated"));
	entries_ += Entry(Mood::Hungry, "hungry", QObject::tr("Hungry"));
	entries_ += Entry(Mood::Hurt, "hurt", QObject::tr("Hurt"));
	entries_ += Entry(Mood::Impressed, "impressed", QObject::tr("Impressed"));
	entries_ += Entry(Mood::In_awe, "in_awe", QObject::tr("In Awe"));
	entries_ += Entry(Mood::In_love, "in_love", QObject::tr("In Love"));
	entries_ += Entry(Mood::Indignant, "indignant", QObject::tr("Indignant"));
	entries_ += Entry(Mood::Interested, "interested", QObject::tr("Interested"));
	entries_ += Entry(Mood::Intoxicated, "intoxicated", QObject::tr("Intoxicated"));
	entries_ += Entry(Mood::Invincible, "invincible", QObject::tr("Invincible"));
	entries_ += Entry(Mood::Jealous, "jealous", QObject::tr("Jealous"));
	entries_ += Entry(Mood::Lonely, "lonely", QObject::tr("Lonely"));
	entries_ += Entry(Mood::Lucky, "lucky", QObject::tr("Lucky"));
	entries_ += Entry(Mood::Mean, "mean", QObject::tr("Mean"));
	entries_ += Entry(Mood::Moody, "moody", QObject::tr("Moody"));
	entries_ += Entry(Mood::Nervous, "nervous", QObject::tr("Nervous"));
	entries_ += Entry(Mood::Neutral, "neutral", QObject::tr("Neutral"));
	entries_ += Entry(Mood::Offended, "offended", QObject::tr("Offended"));
	entries_ += Entry(Mood::Outraged, "outraged", QObject::tr("Outraged"));
	entries_ += Entry(Mood::Playful, "playful", QObject::tr("Playful"));
	entries_ += Entry(Mood::Proud, "proud", QObject::tr("Proud"));
	entries_ += Entry(Mood::Relaxed, "relaxed", QObject::tr("Relaxed"));
	entries_ += Entry(Mood::Relieved, "relieved", QObject::tr("Relieved"));
	entries_ += Entry(Mood::Remorseful, "remorseful", QObject::tr("Remorseful"));
	entries_ += Entry(Mood::Restless, "restless", QObject::tr("Restless"));
	entries_ += Entry(Mood::Sad, "sad", QObject::tr("Sad"));
	entries_ += Entry(Mood::Sarcastic, "sarcastic", QObject::tr("Sarcastic"));
	entries_ += Entry(Mood::Serious, "serious", QObject::tr("Serious"));
	entries_ += Entry(Mood::Shocked, "shocked", QObject::tr("Shocked"));
	entries_ += Entry(Mood::Shy, "shy", QObject::tr("Shy"));
	entries_ += Entry(Mood::Sick, "sick", QObject::tr("Sick"));
	entries_ += Entry(Mood::Sleepy, "sleepy", QObject::tr("Sleepy"));
	entries_ += Entry(Mood::Spontaneous, "spontaneous", QObject::tr("Spontaneous"));
	entries_ += Entry(Mood::Stressed, "stressed", QObject::tr("Stressed"));
	entries_ += Entry(Mood::Strong, "strong", QObject::tr("Strong"));
	entries_ += Entry(Mood::Surprised, "surprised", QObject::tr("Surprised"));
	entries_ += Entry(Mood::Thankful, "thankful", QObject::tr("Thankful"));
	entries_ += Entry(Mood::Thirsty, "thirsty", QObject::tr("Thirsty"));
	entries_ += Entry(Mood::Tired, "tired", QObject::tr("Tired"));
	entries_ += Entry(Mood::Undefined, "undefined", QObject::tr("Else"));
	entries_ += Entry(Mood::Weak, "weak", QObject::tr("Weak"));
	entries_ += Entry(Mood::Worried, "worried", QObject::tr("Worried"));
	qSort(entries_);
}

MoodCatalog* MoodCatalog::instance()
{
	if (!instance_)
		instance_ = new  MoodCatalog();
	return instance_;
}

MoodCatalog::Entry MoodCatalog::findEntryByType(Mood::Type type) const
{
	foreach(Entry e, entries_) {
		if (e.type() == type)
			return e;
	}
	return Entry();
}

MoodCatalog::Entry MoodCatalog::findEntryByValue(const QString& value) const
{
	foreach(Entry e, entries_) {
		if (e.value() == value)
			return e;
	}
	return Entry();
}

MoodCatalog::Entry MoodCatalog::findEntryByText(const QString& text) const
{
	foreach(Entry e, entries_) {
		if (e.text() == text)
			return e;
	}
	return Entry();
}

const QList<MoodCatalog::Entry>& MoodCatalog::entries() const
{
	return entries_;
}

MoodCatalog* MoodCatalog::instance_ = 0;
