#include "jinglertp.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include <QtCrypto>
#include "psimedia.h"
#include "ui_config.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "xmpp_xmlcommon.h"
#include "psiaccount.h"
#include "iris/ice176.h"

static QChar randomPrintableChar()
{
	// 0-25 = a-z
	// 26-51 = A-Z
	// 52-61 = 0-9

	uchar c = QCA::Random::randomChar() % 62;
	if(c <= 25)
		return 'a' + c;
	else if(c <= 51)
		return 'A' + (c - 26);
	else
		return '0' + (c - 52);
}

static QString randomCredential(int len)
{
	QString out;
	for(int n = 0; n < len; ++n)
		out += randomPrintableChar();
	return out;
}

static QDomElement candidateToElement(QDomDocument *doc, const XMPP::Ice176::Candidate &c)
{
	QDomElement e = doc->createElement("candidate");
	e.setAttribute("component", QString::number(c.component));
	e.setAttribute("foundation", c.foundation);
	e.setAttribute("generation", QString::number(c.generation));
	if(!c.id.isEmpty())
		e.setAttribute("id", c.id);
	e.setAttribute("ip", c.ip.toString());
	if(c.network != -1)
		e.setAttribute("network", QString::number(c.network));
	else // weird?
		e.setAttribute("network", QString::number(0));
	e.setAttribute("port", QString::number(c.port));
	e.setAttribute("priority", QString::number(c.priority));
	e.setAttribute("protocol", c.protocol);
	if(!c.rel_addr.isNull())
		e.setAttribute("rel-addr", c.rel_addr.toString());
	if(c.rel_port != -1)
		e.setAttribute("rel-port", QString::number(c.rel_port));
	if(!c.rem_addr.isNull())
		e.setAttribute("rem-addr", c.rem_addr.toString());
	if(c.rem_port != -1)
		e.setAttribute("rem-port", QString::number(c.rem_port));
	e.setAttribute("type", c.type);
	return e;
}

static XMPP::Ice176::Candidate elementToCandidate(const QDomElement &e)
{
	if(e.tagName() != "candidate")
		return XMPP::Ice176::Candidate();

	XMPP::Ice176::Candidate c;
	c.component = e.attribute("component").toInt();
	c.foundation = e.attribute("foundation");
	c.generation = e.attribute("generation").toInt();
	c.id = e.attribute("id");
	c.ip = QHostAddress(e.attribute("ip"));
	c.network = e.attribute("network").toInt();
	c.port = e.attribute("port").toInt();
	c.priority = e.attribute("priority").toInt();
	c.protocol = e.attribute("protocol");
	c.rel_addr = QHostAddress(e.attribute("rel-addr"));
	c.rel_port = e.attribute("rel-port").toInt();
	c.rem_addr = QHostAddress(e.attribute("rem-addr"));
	c.rem_port = e.attribute("rem-port").toInt();
	c.type = e.attribute("type");
	return c;
}

static QDomElement payloadInfoToElement(QDomDocument *doc, const PsiMedia::PayloadInfo &info)
{
	QDomElement e = doc->createElement("payload-type");
	e.setAttribute("id", QString::number(info.id()));
	if(!info.name().isEmpty())
		e.setAttribute("name", info.name());
	e.setAttribute("clockrate", QString::number(info.clockrate()));
	if(info.channels() > 1)
		e.setAttribute("channels", QString::number(info.channels()));
	if(info.ptime() != -1)
		e.setAttribute("ptime", QString::number(info.ptime()));
	if(info.maxptime() != -1)
		e.setAttribute("maxptime", QString::number(info.maxptime()));
	foreach(const PsiMedia::PayloadInfo::Parameter &p, info.parameters())
	{
		QDomElement pe = doc->createElement("parameter");
		pe.setAttribute("name", p.name);
		pe.setAttribute("value", p.value);
		e.appendChild(pe);
	}
	return e;
}

static PsiMedia::PayloadInfo elementToPayloadInfo(const QDomElement &e)
{
	if(e.tagName() != "payload-type")
		return PsiMedia::PayloadInfo();

	PsiMedia::PayloadInfo out;
	bool ok;
	int x;

	x = e.attribute("id").toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setId(x);

	out.setName(e.attribute("name"));

	x = e.attribute("clockrate").toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setClockrate(x);

	x = e.attribute("channels").toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setChannels(x);

	x = e.attribute("ptime").toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setPtime(x);

	x = e.attribute("maxptime").toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setMaxptime(x);

	QList<PsiMedia::PayloadInfo::Parameter> plist;
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
	{
		if(!n.isElement())
			continue;

		QDomElement pe = n.toElement();
		PsiMedia::PayloadInfo::Parameter p;
		p.name = pe.attribute("name");
		p.value = pe.attribute("value");
		plist += p;
	}
	out.setParameters(plist);

	return out;
}

Q_DECLARE_METATYPE(PsiMedia::AudioParams);
Q_DECLARE_METATYPE(PsiMedia::VideoParams);

class Configuration
{
public:
	bool liveInput;
	QString audioOutDeviceId, audioInDeviceId, videoInDeviceId;
	QString file;
	bool loopFile;
	PsiMedia::AudioParams audioParams;
	PsiMedia::VideoParams videoParams;

	Configuration() :
		liveInput(false),
		loopFile(false)
	{
	}
};

class PsiMediaFeaturesSnapshot
{
public:
	QList<PsiMedia::Device> audioOutputDevices;
	QList<PsiMedia::Device> audioInputDevices;
	QList<PsiMedia::Device> videoInputDevices;
	QList<PsiMedia::AudioParams> supportedAudioModes;
	QList<PsiMedia::VideoParams> supportedVideoModes;

	PsiMediaFeaturesSnapshot()
	{
		PsiMedia::Features f;
		f.lookup();
		f.waitForFinished();

		audioOutputDevices = f.audioOutputDevices();
		audioInputDevices = f.audioInputDevices();
		videoInputDevices = f.videoInputDevices();
		supportedAudioModes = f.supportedAudioModes();
		supportedVideoModes = f.supportedVideoModes();
	}
};

// get default settings
static Configuration getDefaultConfiguration()
{
	Configuration config;
	config.liveInput = true;
	config.loopFile = true;

	PsiMediaFeaturesSnapshot snap;

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

	config.audioParams = snap.supportedAudioModes.first();
	config.videoParams = snap.supportedVideoModes.first();

	return config;
}

// adjust any invalid settings to nearby valid ones
static Configuration adjustConfiguration(const Configuration &in, const PsiMediaFeaturesSnapshot &snap)
{
	Configuration out = in;
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

	found = false;
	foreach(const PsiMedia::AudioParams &params, snap.supportedAudioModes)
	{
		if(out.audioParams == params)
		{
			found = true;
			break;
		}
	}
	if(!found)
		out.audioParams = snap.supportedAudioModes.first();

	found = false;
	foreach(const PsiMedia::VideoParams &params, snap.supportedVideoModes)
	{
		if(out.videoParams == params)
		{
			found = true;
			break;
		}
	}
	if(!found)
		out.videoParams = snap.supportedVideoModes.first();

	return out;
}

class ConfigDlg : public QDialog
{
	Q_OBJECT

public:
	Ui::Config ui;
	Configuration config;

	ConfigDlg(const Configuration &_config, QWidget *parent = 0) :
		QDialog(parent),
		config(_config)
	{
		ui.setupUi(this);
		setWindowTitle(tr("Configure Audio/Video"));

		ui.lb_audioInDevice->setEnabled(false);
		ui.cb_audioInDevice->setEnabled(false);
		ui.lb_videoInDevice->setEnabled(false);
		ui.cb_videoInDevice->setEnabled(false);
		ui.lb_file->setEnabled(false);
		ui.le_file->setEnabled(false);
		ui.tb_file->setEnabled(false);
		ui.ck_loop->setEnabled(false);

		connect(ui.rb_sendLive, SIGNAL(toggled(bool)), SLOT(live_toggled(bool)));
		connect(ui.rb_sendFile, SIGNAL(toggled(bool)), SLOT(file_toggled(bool)));
		connect(ui.tb_file, SIGNAL(clicked()), SLOT(file_choose()));

		PsiMediaFeaturesSnapshot snap;

		ui.cb_audioOutDevice->addItem("<None>", QString());
		foreach(const PsiMedia::Device &dev, snap.audioOutputDevices)
			ui.cb_audioOutDevice->addItem(dev.name(), dev.id());

		ui.cb_audioInDevice->addItem("<None>", QString());
		foreach(const PsiMedia::Device &dev, snap.audioInputDevices)
			ui.cb_audioInDevice->addItem(dev.name(), dev.id());

		ui.cb_videoInDevice->addItem("<None>", QString());
		foreach(const PsiMedia::Device &dev, snap.videoInputDevices)
			ui.cb_videoInDevice->addItem(dev.name(), dev.id());

		foreach(const PsiMedia::AudioParams &params, snap.supportedAudioModes)
		{
			QString codec = params.codec();
			if(codec == "vorbis" || codec == "speex")
				codec[0] = codec[0].toUpper();
			else
				codec = codec.toUpper();
			QString hz = QString::number(params.sampleRate() / 1000);
			QString chanstr;
			if(params.channels() == 1)
				chanstr = "Mono";
			else if(params.channels() == 2)
				chanstr = "Stereo";
			else
				chanstr = QString("Channels: %1").arg(params.channels());
			QString str = QString("%1, %2KHz, %3-bit, %4").arg(codec).arg(hz).arg(params.sampleSize()).arg(chanstr);

			ui.cb_audioMode->addItem(str, qVariantFromValue<PsiMedia::AudioParams>(params));
		}

		foreach(const PsiMedia::VideoParams &params, snap.supportedVideoModes)
		{
			QString codec = params.codec();
			if(codec == "theora")
				codec[0] = codec[0].toUpper();
			else
				codec = codec.toUpper();
			QString sizestr = QString("%1x%2").arg(params.size().width()).arg(params.size().height());
			QString str = QString("%1, %2 @ %3fps").arg(codec).arg(sizestr).arg(params.fps());

			ui.cb_videoMode->addItem(str, qVariantFromValue<PsiMedia::VideoParams>(params));
		}

		// the following lookups are guaranteed, since the config is
		//   adjusted to all valid values as necessary
		config = adjustConfiguration(config, snap);
		ui.cb_audioOutDevice->setCurrentIndex(ui.cb_audioOutDevice->findData(config.audioOutDeviceId));
		ui.cb_audioInDevice->setCurrentIndex(ui.cb_audioInDevice->findData(config.audioInDeviceId));
		ui.cb_videoInDevice->setCurrentIndex(ui.cb_videoInDevice->findData(config.videoInDeviceId));
		ui.cb_audioMode->setCurrentIndex(findAudioParamsData(ui.cb_audioMode, config.audioParams));
		ui.cb_videoMode->setCurrentIndex(findVideoParamsData(ui.cb_videoMode, config.videoParams));
		if(config.liveInput)
			ui.rb_sendLive->setChecked(true);
		else
			ui.rb_sendFile->setChecked(true);
		ui.le_file->setText(config.file);
		ui.ck_loop->setChecked(config.loopFile);
	}

	// apparently custom QVariants can't be compared, so we have to
	//   make our own find functions for the comboboxes
	int findAudioParamsData(QComboBox *cb, const PsiMedia::AudioParams &params)
	{
		for(int n = 0; n < cb->count(); ++n)
		{
			if(qVariantValue<PsiMedia::AudioParams>(cb->itemData(n)) == params)
				return n;
		}

		return -1;
	}

	int findVideoParamsData(QComboBox *cb, const PsiMedia::VideoParams &params)
	{
		for(int n = 0; n < cb->count(); ++n)
		{
			if(qVariantValue<PsiMedia::VideoParams>(cb->itemData(n)) == params)
				return n;
		}

		return -1;
	}

protected:
	virtual void accept()
	{
		config.audioOutDeviceId = ui.cb_audioOutDevice->itemData(ui.cb_audioOutDevice->currentIndex()).toString();
		config.audioInDeviceId = ui.cb_audioInDevice->itemData(ui.cb_audioInDevice->currentIndex()).toString();
		config.audioParams = qVariantValue<PsiMedia::AudioParams>(ui.cb_audioMode->itemData(ui.cb_audioMode->currentIndex()));
		config.videoInDeviceId = ui.cb_videoInDevice->itemData(ui.cb_videoInDevice->currentIndex()).toString();
		config.videoParams = qVariantValue<PsiMedia::VideoParams>(ui.cb_videoMode->itemData(ui.cb_videoMode->currentIndex()));
		config.liveInput = ui.rb_sendLive->isChecked();
		config.file = ui.le_file->text();
		config.loopFile = ui.ck_loop->isChecked();

		QDialog::accept();
	}

private slots:
	void live_toggled(bool on)
	{
		ui.lb_audioInDevice->setEnabled(on);
		ui.cb_audioInDevice->setEnabled(on);
		ui.lb_videoInDevice->setEnabled(on);
		ui.cb_videoInDevice->setEnabled(on);
	}

	void file_toggled(bool on)
	{
		ui.lb_file->setEnabled(on);
		ui.le_file->setEnabled(on);
		ui.tb_file->setEnabled(on);
		ui.ck_loop->setEnabled(on);
	}

	void file_choose()
	{
		QString fileName = QFileDialog::getOpenFileName(this,
			tr("Open File"),
			QCoreApplication::applicationDirPath(),
			tr("Ogg Audio/Video (*.oga *.ogv *.ogg)"));
		if(!fileName.isEmpty())
			ui.le_file->setText(fileName);
	}
};

using namespace XMPP;

class RtpDesc
{
public:
	QString media;
	QList<PsiMedia::PayloadInfo> info;
};

class RtpTrans
{
public:
	QString user;
	QString pass;

	// for transport-info and session-accept
	QList<Ice176::Candidate> candidates;
};

class RtpContent
{
public:
	QString name;
	RtpDesc desc;
	RtpTrans trans;
};

class JT_JingleRtp : public Task
{
	Q_OBJECT

public:
	QDomElement iq;
	Jid to;

	JT_JingleRtp(Task *parent) :
		Task(parent)
	{
	}

	~JT_JingleRtp()
	{
	}

	void initiate(const Jid &_to, const QString &sid, const QList<RtpContent> &contentList)
	{
/*
<iq from='romeo@montague.lit/orchard'
    id='jingle1'
    to='juliet@capulet.lit/balcony'
    type='set'>
  <jingle xmlns='urn:xmpp:jingle:0'
          action='session-initiate'
          initiator='romeo@montague.lit/orchard'
          sid='a73sjjvkla37jfea'>
    <content creator='initiator' name='voice'>
      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>
        <payload-type id='96' name='speex' clockrate='16000'/>
      </description>
      <transport xmlns='urn:xmpp:jingle:transports:ice-udp:0'
                 pwd='asd88fgpdd777uzjYhagZg'
                 ufrag='8hhy'/>
    </content>
  </jingle>
</iq>
*/
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("jingle");
		query.setAttribute("xmlns", "urn:xmpp:jingle:0");
		query.setAttribute("action", "session-initiate");
		query.setAttribute("initiator", client()->jid().full());
		query.setAttribute("sid", sid);
		foreach(const RtpContent &c, contentList)
		{
			QDomElement content = doc()->createElement("content");
			content.setAttribute("creator", "initiator");
			content.setAttribute("name", c.name);

			QDomElement description = doc()->createElement("description");
			description.setAttribute("xmlns", "urn:xmpp:jingle:apps:rtp:1");
			description.setAttribute("media", c.desc.media);
			foreach(const PsiMedia::PayloadInfo &pi, c.desc.info)
			{
				QDomElement p = payloadInfoToElement(doc(), pi);
				if(!p.isNull())
					description.appendChild(p);
			}
			content.appendChild(description);

			QDomElement transport = doc()->createElement("transport");
			transport.setAttribute("xmlns", "urn:xmpp:jingle:transports:ice-udp:0");
			transport.setAttribute("ufrag", c.trans.user);
			transport.setAttribute("pwd", c.trans.pass);
			content.appendChild(transport);

			query.appendChild(content);
		}
		iq.appendChild(query);
	}

	void transportInfo(const Jid &_to, const Jid &initiator, const QString &sid, const QList<RtpContent> &contentList)
	{
/*
<iq from='romeo@montague.lit/orchard'
    id='info1'
    to='juliet@capulet.lit/balcony'
    type='set'>
  <jingle xmlns='urn:xmpp:jingle:0'
          action='transport-info'
          initiator='romeo@montague.lit/orchard'
          sid='a73sjjvkla37jfea'>
    <content creator='initiator' name='voice'>
      <transport xmlns='urn:xmpp:jingle:transports:ice-udp:0'
                 pwd='asd88fgpdd777uzjYhagZg'
                 ufrag='8hhy'>
        <candidate component='1'
                   foundation='1'
                   generation='0'
                   id='el0747fg11'
                   ip='10.0.1.1'
                   network='1'
                   port='8998'
                   priority='2130706431'
                   protocol='udp'
                   type='host'/>
      </transport>
    </content>
  </jingle>
</iq>
*/
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("jingle");
		query.setAttribute("xmlns", "urn:xmpp:jingle:0");
		query.setAttribute("action", "transport-info");
		query.setAttribute("initiator", initiator.full());
		query.setAttribute("sid", sid);
		foreach(const RtpContent &c, contentList)
		{
			QDomElement content = doc()->createElement("content");
			content.setAttribute("creator", "initiator");
			content.setAttribute("name", c.name);

			QDomElement transport = doc()->createElement("transport");
			transport.setAttribute("xmlns", "urn:xmpp:jingle:transports:ice-udp:0");
			transport.setAttribute("ufrag", c.trans.user);
			transport.setAttribute("pwd", c.trans.pass);

			foreach(const Ice176::Candidate &ic, c.trans.candidates)
			{
				QDomElement e = candidateToElement(doc(), ic);
				if(!e.isNull())
					transport.appendChild(e);
			}

			content.appendChild(transport);

			query.appendChild(content);
		}
		iq.appendChild(query);
	}

	void accept(const Jid &_to, const Jid &initiator, const QString &sid, const QList<RtpContent> &contentList)
	{
/*
<iq from='juliet@capulet.lit/balcony'
    id='accept1'
    to='romeo@montague.lit/orchard'
    type='set'>
  <jingle xmlns='urn:xmpp:jingle:0'
          action='session-accept'
          initiator='romeo@montague.lit/orchard'
          responder='juliet@capulet.lit/balcony'
          sid='a73sjjvkla37jfea'>
    <content creator='initiator' name='voice'>
      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>
        <payload-type id='96' name='speex' clockrate='16000'/>
      </description>
      <transport xmlns='urn:xmpp:jingle:transports:ice-udp:0'>
        <candidate component='1'
                   foundation='1'
                   generation='0'
                   ip='192.0.2.3'
                   network='1'
                   port='45664'
                   priority='1694498815'
                   protocol='udp'
                   pwd='asd88fgpdd777uzjYhagZg'
                   rel-addr='10.0.1.1'
                   rel-port='8998'
                   rem-addr='192.0.2.1'
                   rem-port='3478'
                   type='srflx'
                   ufrag='8hhy'/>
      </transport>
    </content>
  </jingle>
</iq>
*/
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("jingle");
		query.setAttribute("xmlns", "urn:xmpp:jingle:0");
		query.setAttribute("action", "session-accept");
		query.setAttribute("initiator", initiator.full());
		query.setAttribute("sid", sid);
		foreach(const RtpContent &c, contentList)
		{
			QDomElement content = doc()->createElement("content");
			content.setAttribute("creator", "initiator");
			content.setAttribute("name", c.name);

			QDomElement description = doc()->createElement("description");
			description.setAttribute("xmlns", "urn:xmpp:jingle:apps:rtp:1");
			description.setAttribute("media", c.desc.media);
			foreach(const PsiMedia::PayloadInfo &pi, c.desc.info)
			{
				QDomElement p = payloadInfoToElement(doc(), pi);
				if(!p.isNull())
					description.appendChild(p);
			}
			content.appendChild(description);

			QDomElement transport = doc()->createElement("transport");
			transport.setAttribute("xmlns", "urn:xmpp:jingle:transports:ice-udp:0");
			transport.setAttribute("ufrag", c.trans.user);
			transport.setAttribute("pwd", c.trans.pass);

			foreach(const Ice176::Candidate &ic, c.trans.candidates)
			{
				QDomElement e = candidateToElement(doc(), ic);
				if(!e.isNull())
					transport.appendChild(e);
			}

			content.appendChild(transport);

			query.appendChild(content);
		}
		iq.appendChild(query);
	}

	void terminate(const Jid &_to, const Jid &initiator, const QString &sid)
	{
/*
<iq from='juliet@capulet.lit/balcony'
    id='term1'
    to='romeo@montague.lit/orchard'
    type='set'>
  <jingle xmlns='urn:xmpp:jingle:0'
          action='session-terminate'
          initiator='romeo@montague.lit/orchard'
          sid='a73sjjvkla37jfea'>
    <reason>
      <success/>
      <text>Sorry, gotta go!</text>
    </reason>
  </jingle>
</iq>
*/
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("jingle");
		query.setAttribute("xmlns", "urn:xmpp:jingle:0");
		query.setAttribute("action", "session-terminate");
		query.setAttribute("initiator", initiator.full());
		query.setAttribute("sid", sid);
		QDomElement reason = doc()->createElement("reason");
		QDomElement success = doc()->createElement("success");
		reason.appendChild(success);
		reason.appendChild(textTag(doc(), "text", "Sorry, gotta go!"));
		query.appendChild(reason);
		iq.appendChild(query);
	}

	void onGo()
	{
		send(iq);
	}

	bool take(const QDomElement &x)
	{
		if(!iqVerify(x, to, id()))
			return false;

		if(x.attribute("type") == "result")
			setSuccess();
		else
			setError(x);

		return true;
	}
};

class RtpPush
{
public:
	Jid from;
	QString sid;
	QList<RtpContent> contentList;
	QString iq_id;
};

class JT_PushJingleRtp : public Task
{
	Q_OBJECT

public:
	JT_PushJingleRtp(Task *parent) :
		Task(parent)
	{
	}

	~JT_PushJingleRtp()
	{
	}

	void respondSuccess(const Jid &to, const QString &id)
	{
		QDomElement iq = createIQ(doc(), "result", to.full(), id);
		send(iq);
	}

	void respondError(const Jid &to, const QString &id, int code, const QString &str)
	{
		QDomElement iq = createIQ(doc(), "error", to.full(), id);
		QDomElement err = textTag(doc(), "error", str);
		err.setAttribute("code", QString::number(code));
		iq.appendChild(err);
		send(iq);
	}

	bool take(const QDomElement &e)
	{
		// must be an iq-set tag
		if(e.tagName() != "iq")
			return false;
		if(e.attribute("type") != "set")
			return false;

		QDomElement je;
		for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
		{
			if(!n.isElement())
				continue;

			QDomElement e = n.toElement();
			if(e.tagName() == "jingle" && e.attribute("xmlns") == "urn:xmpp:jingle:0")
			{
				je = e;
				break;
			}
		}

		if(je.isNull())
			return false;

		Jid from = e.attribute("from");
		QString iq_id = e.attribute("id");

		QString action = je.attribute("action");
		if(action != "session-initiate" && action != "transport-info" && action != "session-accept" && action != "session-terminate")
		{
			respondError(from, iq_id, 400, QString());
			return true;
		}

		RtpPush push;
		push.from = from;
		push.iq_id = iq_id;
		push.sid = je.attribute("sid");

		if(action == "session-terminate")
		{
			emit incomingTerminate(push);
			return true;
		}

		for(QDomNode n = je.firstChild(); !n.isNull(); n = n.nextSibling())
		{
			if(!n.isElement())
				continue;

			QDomElement e = n.toElement();
			printf("child: [%s]\n", qPrintable(e.tagName()));
			if(e.tagName() != "content")
				continue;

			RtpContent c;
			c.name = e.attribute("name");

			for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
			{
				if(!n.isElement())
					continue;

				QDomElement e = n.toElement();
				printf("  child: [%s]\n", qPrintable(e.tagName()));
				if(e.tagName() == "description" && e.attribute("xmlns") == "urn:xmpp:jingle:apps:rtp:1")
				{
					c.desc.media = e.attribute("media");

					for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
					{
						if(!n.isElement())
							continue;

						QDomElement e = n.toElement();
						printf("    child: [%s]\n", qPrintable(e.tagName()));
						PsiMedia::PayloadInfo pi = elementToPayloadInfo(e);
						if(!pi.isNull())
							c.desc.info += pi;
					}
				}
				else if(e.tagName() == "transport" && e.attribute("xmlns") == "urn:xmpp:jingle:transports:ice-udp:0")
				{
					c.trans.user = e.attribute("ufrag");
					c.trans.pass = e.attribute("pwd");

					for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
					{
						if(!n.isElement())
							continue;

						QDomElement e = n.toElement();
						printf("    child: [%s]\n", qPrintable(e.tagName()));
						Ice176::Candidate ic = elementToCandidate(e);
						if(!ic.type.isEmpty())
							c.trans.candidates += ic;
					}
				}
				else
				{
					respondError(from, iq_id, 400, QString());
					return true;
				}
			}

			push.contentList += c;
		}

		if(action == "session-initiate")
			emit incomingInitiate(push);
		else if(action == "transport-info")
			emit incomingTransportInfo(push);
		else // session-accept
			emit incomingAccept(push);

		return true;
	}

signals:
	void incomingInitiate(const RtpPush &push);
	void incomingTransportInfo(const RtpPush &push);
	void incomingAccept(const RtpPush &push);
	void incomingTerminate(const RtpPush &push);
};

//----------------------------------------------------------------------------
// JingleRtpSession
//----------------------------------------------------------------------------
class JingleRtpManagerPrivate : public QObject
{
	Q_OBJECT

public:
	JingleRtpManager *q;

	PsiAccount *pa;
	QHostAddress self_addr;
	JingleRtpSession *sess_out, *sess_in;
	Configuration config;
	JT_PushJingleRtp *push_task;
	JT_JingleRtp *task;

	JingleRtpManagerPrivate(PsiAccount *_pa, JingleRtpManager *_q);
	~JingleRtpManagerPrivate();

	void connectToJid(const Jid &jid, const QString &sid, const QList<RtpContent> &contentList);
	void accept();
	void reject_in();
	void reject_out();

private slots:
	void task_finished();
	void push_task_incomingInitiate(const RtpPush &push);
	void push_task_incomingTransportInfo(const RtpPush &push);
	void push_task_incomingAccept(const RtpPush &push);
	void push_task_incomingTerminate(const RtpPush &push);
};

class JingleRtpSessionPrivate : public QObject
{
	Q_OBJECT

public:
	JingleRtpSession *q;

	bool incoming;
	JingleRtpManager *manager;
	XMPP::Jid jid, init_jid;
	QString iq_id;
	Ice176 *ice;
	PsiMedia::RtpSession producer;
	PsiMedia::RtpSession receiver;
	QString sid;
	QList<RtpContent> contentList;
	bool prov_accepted;
	QList<Ice176::Candidate> localCandidates;
	bool transmitAudio, transmitVideo, transmitting;
	bool receiveAudio, receiveVideo;

	class Channel
	{
	public:
		bool ready;
	};
	QList<Channel> channels;

	JingleRtpSessionPrivate(JingleRtpSession *_q) :
		QObject(_q),
		q(_q),
		producer(this),
		receiver(this),
		prov_accepted(false),
		transmitAudio(false),
		transmitVideo(false),
		transmitting(false),
		receiveAudio(false),
		receiveVideo(false)
	{
		connect(&producer, SIGNAL(started()), SLOT(producer_started()));
		connect(&producer, SIGNAL(stopped()), SLOT(producer_stopped()));
		connect(&producer, SIGNAL(error()), SLOT(producer_error()));
		connect(&receiver, SIGNAL(started()), SLOT(receiver_started()));
		connect(&receiver, SIGNAL(stopped()), SLOT(receiver_stopped()));
		connect(&receiver, SIGNAL(error()), SLOT(receiver_error()));

		ice = new Ice176(this);
		connect(ice, SIGNAL(started()), SLOT(ice_started()));
		connect(ice, SIGNAL(localCandidatesReady(const QList<XMPP::Ice176::Candidate> &)), SLOT(ice_localCandidatesReady(const QList<XMPP::Ice176::Candidate> &)));
		connect(ice, SIGNAL(componentReady(int)), SLOT(ice_componentReady(int)));
		connect(ice, SIGNAL(readyRead(int)), SLOT(ice_readyRead(int)));
		connect(ice, SIGNAL(datagramsWritten(int, int)), SLOT(ice_datagramsWritten(int, int)));
	}

	~JingleRtpSessionPrivate()
	{
		printf("~JingleRtpSessionPrivate\n");
		//producer.stop();
		//receiver.stop();

		ice->disconnect(this);
		ice->setParent(0);
		ice->deleteLater();
	}

	static QString rtpSessionErrorToString(PsiMedia::RtpSession::Error e)
	{
		QString str;
		switch(e)
		{
			case PsiMedia::RtpSession::ErrorSystem:
				str = tr("System error"); break;
			case PsiMedia::RtpSession::ErrorCodec:
				str = tr("Codec error"); break;
			default: // generic
				str = tr("Generic error"); break;
		}
		return str;
	}

	void prov_accept()
	{
		prov_accepted = true;

		if(!localCandidates.isEmpty())
		{
			RtpContent c;
			c.name = "A";
			c.trans.user = ice->localUfrag();
			c.trans.pass = ice->localPassword();
			c.trans.candidates = localCandidates;
			localCandidates.clear();
			sendTransportInfo(c);
		}

		transmit();
		QTimer::singleShot(1000, this, SLOT(start_receive()));
	}

	void sendTransportInfo(const RtpContent &content)
	{
		JT_JingleRtp *jt = new JT_JingleRtp(manager->d->pa->client()->rootTask());
		jt->transportInfo(jid, init_jid, sid, QList<RtpContent>() << content);
		jt->go(true);
	}

	void getTransportInfo(const QList<RtpContent> &contentList)
	{
		printf("getTransportInfo: %d\n", contentList.count());

		// FIXME: assumes on content type that exists
		ice->setPeerUfrag(contentList[0].trans.user);
		ice->setPeerPassword(contentList[0].trans.pass);
		ice->addRemoteCandidates(contentList[0].trans.candidates);
	}

	void incomingAccept(const QList<RtpContent> &contentList)
	{
		Q_UNUSED(contentList);
		emit q->activated();
	}

public slots:
	void start_send()
	{
		manager->d->config = adjustConfiguration(manager->d->config, PsiMediaFeaturesSnapshot());
		manager->d->config.videoInDeviceId.clear(); // ### disabling video
		Configuration &config = manager->d->config;

		transmitAudio = false;
		transmitVideo = false;

		if(config.liveInput)
		{
			if(config.audioInDeviceId.isEmpty() && config.videoInDeviceId.isEmpty())
			{
				QMessageBox::information(0, tr("Error"), tr(
					"Cannot send live without at least one audio "
					"input or video input device selected."
					));
				return;
			}

			if(!config.audioInDeviceId.isEmpty())
			{
				producer.setAudioInputDevice(config.audioInDeviceId);
				transmitAudio = true;
			}
			else
				producer.setAudioInputDevice(QString());

			if(!config.videoInDeviceId.isEmpty())
			{
				producer.setVideoInputDevice(config.videoInDeviceId);
				transmitVideo = true;
			}
			else
				producer.setVideoInputDevice(QString());
		}
		else // non-live (file) input
		{
			producer.setFileInput(config.file);
			producer.setFileLoopEnabled(config.loopFile);

			// we just assume the file has both audio and video.
			//   if it doesn't, no big deal, it'll still work.
			//   update: after producer is started, we can correct
			//   these variables.
			transmitAudio = true;
			transmitVideo = true;
		}

		QList<PsiMedia::AudioParams> audioParamsList;
		if(transmitAudio)
			audioParamsList += config.audioParams;
		producer.setLocalAudioPreferences(audioParamsList);

		QList<PsiMedia::VideoParams> videoParamsList;
		if(transmitVideo)
			videoParamsList += config.videoParams;
		producer.setLocalVideoPreferences(videoParamsList);

		transmitting = false;
		producer.start();
	}

	void transmit()
	{
		if(transmitAudio)
			producer.transmitAudio();
		if(transmitVideo)
			producer.transmitVideo();

		transmitting = true;
	}

	void stop_send()
	{
		producer.stop();
	}

	void start_receive()
	{
		Configuration &config = manager->d->config;

		PsiMedia::PayloadInfo audio;
		PsiMedia::PayloadInfo video;

		for(int n = 0; n < contentList.count(); ++n)
		{
			if(contentList[n].desc.media == "audio")
				audio = contentList[n].desc.info.first();
			else if(contentList[n].desc.media == "video")
				video = contentList[n].desc.info.first();
		}

		receiveAudio = !audio.isNull();
		receiveVideo = !video.isNull();

		if(receiveAudio && !config.audioOutDeviceId.isEmpty())
		{
			receiver.setAudioOutputDevice(config.audioOutDeviceId);

			QList<PsiMedia::AudioParams> audioParamsList;
			audioParamsList += config.audioParams;
			receiver.setLocalAudioPreferences(audioParamsList);

			QList<PsiMedia::PayloadInfo> payloadInfoList;
			payloadInfoList += audio;
			receiver.setRemoteAudioPreferences(payloadInfoList);
		}

		if(receiveVideo)
		{
			QList<PsiMedia::VideoParams> videoParamsList;
			videoParamsList += config.videoParams;
			receiver.setLocalVideoPreferences(videoParamsList);

			QList<PsiMedia::PayloadInfo> payloadInfoList;
			payloadInfoList += video;
			receiver.setRemoteVideoPreferences(payloadInfoList);
		}

		receiver.start();
	}

	void stop_receive()
	{
		receiver.stop();
	}

	void producer_started()
	{
		printf("producer_started\n");

		PsiMedia::PayloadInfo audio, *pAudio;
		PsiMedia::PayloadInfo video, *pVideo;

		pAudio = 0;
		pVideo = 0;
		if(transmitAudio)
		{
			// confirm transmitting of audio is actually possible,
			//   in the case that a file is used as input
			if(producer.canTransmitAudio())
			{
				audio = producer.audioPayloadInfo().first();
				pAudio = &audio;
			}
			else
				transmitAudio = false;
		}
		if(transmitVideo)
		{
			// same for video
			if(producer.canTransmitVideo())
			{
				video = producer.videoPayloadInfo().first();
				pVideo = &video;
			}
			else
				transmitVideo = false;
		}

		connect(producer.audioRtpChannel(), SIGNAL(readyRead()), SLOT(rtp_audio_readyRead()));

		contentList.clear();
		if(pAudio)
		{
			RtpContent c;
			c.name = "A";
			c.desc.media = "audio";
			c.desc.info += *pAudio;
			contentList += c;
		}
		if(pVideo)
		{
			RtpContent c;
			c.name = "V";
			c.desc.media = "video";
			c.desc.info += *pVideo;
			contentList += c;
		}

		QList<Ice176::LocalAddress> localAddrs;
		Ice176::LocalAddress addr;
		addr.addr = manager->d->self_addr;
		localAddrs += addr;
		ice->setLocalAddresses(localAddrs);

		for(int n = 0; n < 2; ++n)
		{
			Channel c;
			c.ready = false;
			channels += c;
		}
		ice->setComponentCount(2);

		QHostAddress stunAddr(QString::fromLocal8Bit(qgetenv("PSI_STUNADDR")));
		int stunPort = 3478;
		if(!stunAddr.isNull())
		{
			XMPP::Ice176::StunServiceType stunType;
			//if(opt_is_relay)
			//	stunType = XMPP::Ice176::Relay;
			//else
				stunType = XMPP::Ice176::Basic;
			ice->setStunService(stunType, stunAddr, stunPort);
			/*if(!opt_user.isEmpty())
			{
				ice->setStunUsername(opt_user);
				ice->setStunPassword(opt_pass.toUtf8());
			}*/

			printf("STUN service: %s\n", qPrintable(stunAddr.toString()));
		}

		if(!incoming)
			ice->start(Ice176::Initiator);
		else
			ice->start(Ice176::Responder);
	}

	void producer_stopped()
	{
	}

	void producer_error()
	{
		QMessageBox::critical(0, tr("Error"), tr(
			"An error occurred while trying to send:\n%1."
			).arg(rtpSessionErrorToString(producer.errorCode())
			));
	}

	void receiver_started()
	{
	}

	void receiver_stopped()
	{
	}

	void receiver_error()
	{
		QMessageBox::critical(0, tr("Error"), tr(
			"An error occurred while trying to receive:\n%1."
			).arg(rtpSessionErrorToString(receiver.errorCode())
			));
	}

	void ice_started()
	{
		// outbound?
		if(!incoming)
		{
			contentList[0].trans.user = ice->localUfrag();
			contentList[0].trans.pass = ice->localPassword();

			// start the request
			manager->d->connectToJid(jid, sid, contentList);
		}
		// inbound?
		else
		{
			// accept
			manager->d->push_task->respondSuccess(jid, iq_id);

			prov_accept();
		}
	}

	void ice_localCandidatesReady(const QList<XMPP::Ice176::Candidate> &list)
	{
		if(prov_accepted)
		{
			RtpContent c;
			c.name = "A";
			c.trans.user = ice->localUfrag();
			c.trans.pass = ice->localPassword();
			c.trans.candidates = list;
			sendTransportInfo(c);
		}
		else
			localCandidates += list;
	}

	void ice_componentReady(int index)
	{
		// do we need this check?
		if(channels[index].ready)
			return;

		channels[index].ready = true;

		bool allReady = true;
		foreach(const Channel &c, channels)
		{
			if(!c.ready)
			{
				allReady = false;
				break;
			}
		}

		if(allReady)
		{
			QList<RtpContent> list = contentList;
			list[0].trans.user = ice->localUfrag();
			list[0].trans.pass = ice->localPassword();
			// FIXME: add the used candidates for each component

			JT_JingleRtp *jt = new JT_JingleRtp(manager->d->pa->client()->rootTask());
			jt->transportInfo(jid, init_jid, sid, list);
			jt->go(true);
		}
	}

	void ice_readyRead(int componentIndex)
	{
		QByteArray buf = ice->readDatagram(componentIndex);
		receiver.audioRtpChannel()->write(PsiMedia::RtpPacket(buf, componentIndex));
	}

	void ice_datagramsWritten(int componentIndex, int count)
	{
		// nothing?
		Q_UNUSED(componentIndex);
		Q_UNUSED(count);
	}

	void rtp_audio_readyRead()
	{
		while(producer.audioRtpChannel()->packetsAvailable() > 0)
		{
			PsiMedia::RtpPacket packet = producer.audioRtpChannel()->read();
			ice->writeDatagram(packet.portOffset(), packet.rawValue());
		}
	}
};

JingleRtpSession::JingleRtpSession() :
	QObject(0)
{
	d = new JingleRtpSessionPrivate(this);
}

JingleRtpSession::~JingleRtpSession()
{
	if(d->incoming)
		d->manager->d->sess_in = 0;
	else
		d->manager->d->sess_out = 0;
	delete d;
}

XMPP::Jid JingleRtpSession::jid() const
{
	return d->jid;
}

void JingleRtpSession::connectToJid(const XMPP::Jid &jid)
{
	d->jid = jid;
	d->init_jid = d->manager->d->pa->client()->jid();
	d->sid = randomCredential(16);
	d->start_send();
}

void JingleRtpSession::accept()
{
	d->manager->d->accept();
}

void JingleRtpSession::reject()
{
	if(d->incoming)
		d->manager->d->reject_in();
	else
		d->manager->d->reject_out();
}

void JingleRtpSession::setIncomingVideo(PsiMedia::VideoWidget *widget)
{
	d->receiver.setVideoOutputWidget(widget);
}

//----------------------------------------------------------------------------
// JingleRtpManager
//----------------------------------------------------------------------------
static JingleRtpManager *g_manager = 0;

#ifdef GSTPROVIDER_STATIC
Q_IMPORT_PLUGIN(gstprovider)
#endif

#ifndef GSTPROVIDER_STATIC
static QString findPlugin(const QString &relpath, const QString &basename)
{
	QDir dir(QCoreApplication::applicationDirPath());
	if(!dir.cd(relpath))
		return QString();
	foreach(const QString &fileName, dir.entryList())
	{
		if(fileName.contains(basename))
		{
			QString filePath = dir.filePath(fileName);
			if(QLibrary::isLibrary(filePath))
				return filePath;
		}
	}
	return QString();
}
#endif

JingleRtpManagerPrivate::JingleRtpManagerPrivate(PsiAccount *_pa, JingleRtpManager *_q) :
	QObject(_q),
	q(_q),
	pa(_pa),
	sess_out(0),
	sess_in(0),
	task(0)
{
#ifndef GSTPROVIDER_STATIC
	QString pluginFile = findPlugin(".", "gstprovider");
# ifdef GSTBUNDLE_PATH
	PsiMedia::loadPlugin(pluginFile, GSTBUNDLE_PATH);
# else
	PsiMedia::loadPlugin(pluginFile, QString());
# endif
#endif

	if(!PsiMedia::isSupported())
	{
		QMessageBox::critical(0, tr("PsiMedia Test"),
			tr(
			"Error: Could not load PsiMedia subsystem."
			));
		exit(1);
	}

	config = getDefaultConfiguration();

	push_task = new JT_PushJingleRtp(pa->client()->rootTask());

	connect(push_task, SIGNAL(incomingInitiate(const RtpPush &)), SLOT(push_task_incomingInitiate(const RtpPush &)));
	connect(push_task, SIGNAL(incomingTransportInfo(const RtpPush &)), SLOT(push_task_incomingTransportInfo(const RtpPush &)));
	connect(push_task, SIGNAL(incomingAccept(const RtpPush &)), SLOT(push_task_incomingAccept(const RtpPush &)));
	connect(push_task, SIGNAL(incomingTerminate(const RtpPush &)), SLOT(push_task_incomingTerminate(const RtpPush &)));
}

JingleRtpManagerPrivate::~JingleRtpManagerPrivate()
{
	delete push_task;
}

void JingleRtpManagerPrivate::connectToJid(const Jid &jid, const QString &sid, const QList<RtpContent> &contentList)
{
	task = new JT_JingleRtp(pa->client()->rootTask());
	connect(task, SIGNAL(finished()), SLOT(task_finished()));
	task->initiate(jid, sid, contentList);
	task->go(true);
}

void JingleRtpManagerPrivate::accept()
{
	sess_in->d->start_send();
}

void JingleRtpManagerPrivate::reject_in()
{
	// terminate active session, if any
	JT_JingleRtp *jt = new JT_JingleRtp(pa->client()->rootTask());
	jt->terminate(sess_in->d->jid, sess_in->d->init_jid, sess_in->d->sid);
	jt->go(true);
}

void JingleRtpManagerPrivate::reject_out()
{
	// terminate active session, if any
	JT_JingleRtp *jt = new JT_JingleRtp(pa->client()->rootTask());
	jt->terminate(sess_out->d->jid, sess_out->d->init_jid, sess_out->d->sid);
	jt->go(true);
}

void JingleRtpManagerPrivate::task_finished()
{
	JT_JingleRtp *jt = task;
	task = 0;

	if(!sess_out)
		return;

	if(jt->success())
		sess_out->d->prov_accept();
	else
		emit sess_out->rejected();
}

void JingleRtpManagerPrivate::push_task_incomingInitiate(const RtpPush &push)
{
	printf("incomingInitiate\n");

	if(sess_in)
	{
		printf("rejecting, only one session allowed at a time.\n");
		push_task->respondError(push.from, push.iq_id, 400, QString());
		return;
	}

	sess_in = new JingleRtpSession;
	sess_in->d->manager = q;
	sess_in->d->incoming = true;
	sess_in->d->jid = push.from;
	sess_in->d->init_jid = push.from;
	sess_in->d->sid = push.sid;
	sess_in->d->contentList = push.contentList;
	sess_in->d->iq_id = push.iq_id;
	emit q->incomingReady();
}

void JingleRtpManagerPrivate::push_task_incomingTransportInfo(const RtpPush &push)
{
	printf("incomingTransportInfo\n");

	if(sess_in && push.from.compare(sess_in->d->jid) && push.sid == sess_in->d->sid)
	{
		sess_in->d->getTransportInfo(push.contentList);
		push_task->respondSuccess(push.from, push.iq_id);
	}
	else if(sess_out && push.from.compare(sess_out->d->jid) && push.sid == sess_out->d->sid)
	{
		sess_out->d->getTransportInfo(push.contentList);
		push_task->respondSuccess(push.from, push.iq_id);
	}
	else
		push_task->respondError(push.from, push.iq_id, 400, QString());
}

void JingleRtpManagerPrivate::push_task_incomingAccept(const RtpPush &push)
{
	printf("incomingAccept\n");

	if(sess_in && push.from.compare(sess_in->d->jid) && push.sid == sess_in->d->sid)
	{
		sess_in->d->incomingAccept(push.contentList);
		push_task->respondSuccess(push.from, push.iq_id);
	}
	else if(sess_out && push.from.compare(sess_out->d->jid) && push.sid == sess_out->d->sid)
	{
		sess_out->d->incomingAccept(push.contentList);
		push_task->respondSuccess(push.from, push.iq_id);
	}
	else
		push_task->respondError(push.from, push.iq_id, 400, QString());
}

void JingleRtpManagerPrivate::push_task_incomingTerminate(const RtpPush &push)
{
	printf("incomingTerminate\n");

	if(sess_in && push.from.compare(sess_in->d->jid) && push.sid == sess_in->d->sid)
	{
		emit sess_in->rejected();
	}
	else if(sess_out && push.from.compare(sess_out->d->jid) && push.sid == sess_out->d->sid)
	{
		emit sess_out->rejected();
	}
	else
		push_task->respondError(push.from, push.iq_id, 400, QString());
}

JingleRtpManager::JingleRtpManager(PsiAccount *pa) :
	QObject(0)
{
	g_manager = this;
	d = new JingleRtpManagerPrivate(pa, this);
}

JingleRtpManager::~JingleRtpManager()
{
	delete d;
	g_manager = 0;
}

JingleRtpManager *JingleRtpManager::instance()
{
	return g_manager;
}

JingleRtpSession *JingleRtpManager::createOutgoing()
{
	// only one session allowed at a time
	Q_ASSERT(!d->sess_out);

	JingleRtpSession *sess = new JingleRtpSession;
	d->sess_out = sess;
	sess->d->manager = this;
	sess->d->incoming = false;
	return sess;
}

JingleRtpSession *JingleRtpManager::takeIncoming()
{
	return d->sess_in;
}

void JingleRtpManager::config()
{
	ConfigDlg w(d->config, 0);
	w.exec();
	d->config = w.config;
}

void JingleRtpManager::setSelfAddress(const QHostAddress &addr)
{
	d->self_addr = addr;
}

#include "jinglertp.moc"
