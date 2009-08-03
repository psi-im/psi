#include "ringsound.h"

#include <QTimer>

class RingSound::Private : public QObject
{
	Q_OBJECT

public:
	RingSound *q;

	RingSound::Mode mode;
	int ringCount, ringMax;
	QTimer *t;

	Private(RingSound *_q) :
		QObject(_q),
		q(_q)
	{
		mode = RingSound::Outgoing;

		t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(t_timeout()));
	}

	void start()
	{
		ringCount = 0;

		if(mode == RingSound::Outgoing)
		{
			// for outgoing, start ringing in 2 seconds, for
			//   a "phone feel"
			t->start(3000);
		}
		else
		{
			// for incoming, ring immediately
			t_timeout();
			start_regular_timer();
		}
	}

	void stop()
	{
		// TODO: if there is a sound playing, stop it immediately
		t->stop();
	}

private:
	void start_regular_timer()
	{
		t->start(5000);
	}

private slots:
	void t_timeout()
	{
		if(mode == RingSound::Outgoing)
			printf("...ring...\n");
		else
			printf("*burrriiinngg*\n");

		if(ringCount == 0 && mode == RingSound::Outgoing)
			start_regular_timer();

		if(ringMax >= 1 || (ringMax < 1 && ringCount < 1))
			++ringCount;

		if(ringMax >= 1 && ringCount >= ringMax)
			stop();
	}
};

RingSound::RingSound(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

RingSound::~RingSound()
{
	delete d;
}

void RingSound::setMode(Mode m)
{
	d->mode = m;
}

void RingSound::start(int count)
{
	d->ringMax = count;
	d->start();
}

void RingSound::stop()
{
	d->stop();
}

#include "ringsound.moc"
