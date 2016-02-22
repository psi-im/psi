#include "opt_avcall.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "../psimedia/psimedia.h"
#include "../avcall/avcall.h"

#include <QComboBox>
#include <QLineEdit>
#include <QList>

#include "ui_opt_avcall.h"

class OptAvCallUI : public QWidget, public Ui::OptAvCall
{
public:
	OptAvCallUI() : QWidget() { setupUi(this); }
};

// adapted from psimedia configure dialog
class Configuration2
{
public:
	QString audioOutDeviceId, audioInDeviceId, videoInDeviceId;
};

class PsiMediaFeaturesSnapshot2
{
public:
	QList<PsiMedia::Device> audioOutputDevices;
	QList<PsiMedia::Device> audioInputDevices;
	QList<PsiMedia::Device> videoInputDevices;

	PsiMediaFeaturesSnapshot2()
	{
		PsiMedia::Features f;
		int types = 0;
		types |= PsiMedia::Features::AudioOut;
		types |= PsiMedia::Features::AudioIn;
		if(AvCallManager::isVideoSupported())
			types |= PsiMedia::Features::VideoIn;
		f.lookup(types);
		f.waitForFinished();

		audioOutputDevices = f.audioOutputDevices();
		audioInputDevices = f.audioInputDevices();
		videoInputDevices = f.videoInputDevices();
	}
};

// get default settings
static Configuration2 getDefaultConfiguration(const PsiMediaFeaturesSnapshot2 &snap)
{
	Configuration2 config;

	QList<PsiMedia::Device> devs;

	devs = snap.audioOutputDevices;
	if(!devs.isEmpty())
		config.audioOutDeviceId = devs.first().id();

	devs = snap.audioInputDevices;
	if(!devs.isEmpty())
		config.audioInDeviceId = devs.first().id();

	devs = snap.videoInputDevices;
	if(!devs.isEmpty())
		config.videoInDeviceId = devs.first().id();

	return config;
}

// adjust any invalid settings to nearby valid ones
static Configuration2 adjustConfiguration(const Configuration2 &in, const PsiMediaFeaturesSnapshot2 &snap)
{
	Configuration2 out = in;
	bool found;

	if(!out.audioOutDeviceId.isEmpty())
	{
		found = false;
		foreach(const PsiMedia::Device &dev, snap.audioOutputDevices)
		{
			if(out.audioOutDeviceId == dev.id())
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			if(!snap.audioOutputDevices.isEmpty())
				out.audioOutDeviceId = snap.audioOutputDevices.first().id();
			else
				out.audioOutDeviceId.clear();
		}
	}

	if(!out.audioInDeviceId.isEmpty())
	{
		found = false;
		foreach(const PsiMedia::Device &dev, snap.audioInputDevices)
		{
			if(out.audioInDeviceId == dev.id())
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			if(!snap.audioInputDevices.isEmpty())
				out.audioInDeviceId = snap.audioInputDevices.first().id();
			else
				out.audioInDeviceId.clear();
		}
	}

	if(!out.videoInDeviceId.isEmpty())
	{
		found = false;
		foreach(const PsiMedia::Device &dev, snap.videoInputDevices)
		{
			if(out.videoInDeviceId == dev.id())
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			if(!snap.videoInputDevices.isEmpty())
				out.videoInDeviceId = snap.videoInputDevices.first().id();
			else
				out.videoInDeviceId.clear();
		}
	}

	return out;
}

void options_avcall_update()
{
	PsiMediaFeaturesSnapshot2 snap;

	Configuration2 config = getDefaultConfiguration(snap);
	QString id;
	id = PsiOptions::instance()->getOption("options.media.devices.audio-output").toString();
	if(!id.isEmpty())
		config.audioOutDeviceId = id;
	id = PsiOptions::instance()->getOption("options.media.devices.audio-input").toString();
	if(!id.isEmpty())
		config.audioInDeviceId = id;
	id = PsiOptions::instance()->getOption("options.media.devices.video-input").toString();
	if(!id.isEmpty())
		config.videoInDeviceId = id;

	config = adjustConfiguration(config, snap);
	PsiOptions::instance()->setOption("options.media.devices.audio-output", config.audioOutDeviceId);
	PsiOptions::instance()->setOption("options.media.devices.audio-input", config.audioInDeviceId);
	PsiOptions::instance()->setOption("options.media.devices.video-input", config.videoInDeviceId);
}

//----------------------------------------------------------------------------
// OptionsTabAvCall
//----------------------------------------------------------------------------

OptionsTabAvCall::OptionsTabAvCall(QObject *parent)
: OptionsTab(parent, "avcall", "", tr("Voice Calling"), AvCallManager::isVideoSupported() ? tr("Audio and video device configuration") : tr("Audio device configuration"), "psi/avcall")
{
	w = 0;
}

OptionsTabAvCall::~OptionsTabAvCall()
{
}

QWidget *OptionsTabAvCall::widget()
{
	if ( w )
		return 0;

	w = new OptAvCallUI();
	OptAvCallUI *d = (OptAvCallUI *)w;

	if(!AvCallManager::isVideoSupported()) {
		d->lb_videoInDevice->hide();
		d->cb_videoInDevice->hide();
	}

	return w;
}

void OptionsTabAvCall::applyOptions()
{
	if ( !w )
		return;

	OptAvCallUI *d = (OptAvCallUI *)w;

	PsiOptions::instance()->setOption("options.media.devices.audio-output", d->cb_audioOutDevice->itemData(d->cb_audioOutDevice->currentIndex()).toString());
	PsiOptions::instance()->setOption("options.media.devices.audio-input", d->cb_audioInDevice->itemData(d->cb_audioInDevice->currentIndex()).toString());
	PsiOptions::instance()->setOption("options.media.devices.video-input", d->cb_videoInDevice->itemData(d->cb_videoInDevice->currentIndex()).toString());
	PsiOptions::instance()->setOption("options.media.video-support", d->cb_videoSupport->isChecked());
}

void OptionsTabAvCall::restoreOptions()
{
	if ( !w )
		return;

	OptAvCallUI *d = (OptAvCallUI *)w;

	PsiMediaFeaturesSnapshot2 snap;

	d->cb_audioOutDevice->clear();
	if(snap.audioOutputDevices.isEmpty())
		d->cb_audioOutDevice->addItem("<None>", QString());
	foreach(const PsiMedia::Device &dev, snap.audioOutputDevices)
		d->cb_audioOutDevice->addItem(dev.name(), dev.id());

	d->cb_audioInDevice->clear();
	if(snap.audioInputDevices.isEmpty())
		d->cb_audioInDevice->addItem("<None>", QString());
	foreach(const PsiMedia::Device &dev, snap.audioInputDevices)
		d->cb_audioInDevice->addItem(dev.name(), dev.id());

	d->cb_videoInDevice->clear();
	if(snap.videoInputDevices.isEmpty())
		d->cb_videoInDevice->addItem("<None>", QString());
	foreach(const PsiMedia::Device &dev, snap.videoInputDevices)
		d->cb_videoInDevice->addItem(dev.name(), dev.id());

	Configuration2 config = getDefaultConfiguration(snap);
	QString id;
	id = PsiOptions::instance()->getOption("options.media.devices.audio-output").toString();
	if(!id.isEmpty())
		config.audioOutDeviceId = id;
	id = PsiOptions::instance()->getOption("options.media.devices.audio-input").toString();
	if(!id.isEmpty())
		config.audioInDeviceId = id;
	id = PsiOptions::instance()->getOption("options.media.devices.video-input").toString();
	if(!id.isEmpty())
		config.videoInDeviceId = id;

	config = adjustConfiguration(config, snap);
	d->cb_audioOutDevice->setCurrentIndex(d->cb_audioOutDevice->findData(config.audioOutDeviceId));
	d->cb_audioInDevice->setCurrentIndex(d->cb_audioInDevice->findData(config.audioInDeviceId));
	d->cb_videoInDevice->setCurrentIndex(d->cb_videoInDevice->findData(config.videoInDeviceId));
	d->cb_videoSupport->setChecked(PsiOptions::instance()->getOption("options.media.video-support").toBool());
}
