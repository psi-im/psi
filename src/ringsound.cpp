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
static QSound *soundPlay2(const QString &s, QObject *parent)
{
	QString str = s;
	if(str == "!beep") {
		QApplication::beep();
		return 0;
	}

	if (QDir::isRelativePath(str)) {
		str = ApplicationInfo::resourcesDir() + "/" + str;
	}

	if(!QFile::exists(str))
		return 0;

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
	QSound *sound = new QSound(str, parent);
	sound->play();
	return sound;
#else
	Q_UNUSED(parent);
	QString player = PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString();
	if (player == "") player = soundDetectPlayer();
	QStringList args;
	args = QStringList::split(' ', player);
	args += str;
	QString prog = args.takeFirst();
	QProcess::startDetached(prog, args);
	return 0;
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
		{
			sound->stop();
			sound->setParent(0);
			sound->deleteLater();
			sound = 0;
		}
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
			sound = soundPlay2("sound/ring_outgoing.wav", this);
		}
		else
		{
			//printf("*burrriiinngg*\n");
			sound = soundPlay2("sound/ring_incoming.wav", this);
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
