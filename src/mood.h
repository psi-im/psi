/*
 * mood.h
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

#ifndef MOOD_H
#define MOOD_H

#include <QString>
#include <QMetaType>

#define PEP_MOOD_NS "http://jabber.org/protocol/mood"

class QDomElement;
class QDomDocument;

class Mood
{
public:
	enum Type {
		Unknown,
		Afraid, Amazed, Angry, Amorous, Annoyed, Anxious, Aroused, Ashamed, Bored,
		Brave, Calm, Cautious, Cold, Confident, Confused, Contemplative, Contented,
		Cranky, Crazy, Creative, Curious, Dejected, Depressed, Disappointed,
		Disgusted, Dismayed, Distracted, Embarrassed, Envious, Excited, Flirtatious,
		Frustrated, Grumpy, Guilty, Happy, Hopeful, Hot, Humbled, Humiliated, Hungry,
		Hurt, Impressed, In_awe, In_love, Indignant, Interested, Intoxicated,
		Invincible, Jealous, Lonely, Lucky, Mean, Moody, Nervous, Neutral, Offended,
		Outraged, Playful, Proud, Relaxed, Relieved, Remorseful, Restless, Sad,
		Sarcastic, Serious, Shocked, Shy, Sick, Sleepy, Spontaneous, Stressed, Strong,
		Surprised, Thankful, Thirsty, Tired, Undefined, Weak, Worried
	};

	Mood();
	Mood(Type, const QString& = QString());
	Mood(const QDomElement&);

	Type type() const;
	QString typeText() const;
	QString typeValue() const;
	const QString& text() const;
	bool isNull() const;

	QDomElement toXml(QDomDocument&);

protected:
	void fromXml(const QDomElement&);

private:
	Type type_;
	QString text_;
};

Q_DECLARE_METATYPE(Mood)

#endif
