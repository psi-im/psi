#include "ringsound.h"

#include <QApplication>
#include <QTimer>
#include <QSound>
#include <QFile>
#include <QDir>
#include <QStringList>
#include <QProcess>
#include "applicationinfo.h"
#include "psioptions.h"
#include "common.h"

// adapted from common.h
static void soundPlay2(const QString &s, QSound *sound)
{
	QString str = s;
	if(str == "!beep") {
		QApplication::beep();
		return;
	}

	if (!QDir::isRelativePath(str)) {
		str = ApplicationInfo::resourcesDir() + "/" + str;
	}

	if(!QFile::exists(str))
		return;

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
	if(sound)
		sound->play(str);
#else
	Q_UNUSED(sound);
	QString player = PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString();
	if (player == "") player = soundDetectPlayer();
	QStringList args;
	args = QStringList::split(' ', player);
	args += str;
	QString prog = args.takeFirst();
	QProcess::startDetached(prog, args);
#endif
}

class RingSound::Private : public QObject
{
	Q_OBJECT

public:
	RingSound *q;

	RingSound::Mode mode;
	int ringCount, ringMax;
	QTimer *t;
	QSound *sound;

	Private(RingSound *_q) :
		QObject(_q),
		q(_q),
		sound(0)
	{
		mode = RingSound::Outgoing;

		t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(t_timeout()));

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
		sound = new QSound(this);
#endif
	}

	void start()
	{
		ringCount = 0;

		if(mode == RingSound::Outgoing)
		{
			// for outgoing, start ringing in 3 seconds, for
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
		// if there's a sound playing, stop it immediately.
		// FIXME: make sound stopping work on linux
		if(sound)
			sound->stop();
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
		{
			//printf("...ring...\n");
			soundPlay2("sound/ring_outgoing.wav", sound);
		}
		else
		{
			//printf("*burrriiinngg*\n");
			soundPlay2("sound/ring_incoming.wav", sound);
		}

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
