#ifndef RINGSOUND_H
#define RINGSOUND_H

#include <QObject>

class RingSound : public QObject
{
	Q_OBJECT

public:
	enum Mode
	{
		Outgoing,
		Incoming
	};

	RingSound(QObject *parent = 0);
	~RingSound();

	void setMode(Mode m);

	void start(int count = -1); // -1 = endless ringing
	void stop();

private:
	class Private;
	Private *d;
};

#endif
