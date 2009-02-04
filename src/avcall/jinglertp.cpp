#include "jinglertp.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include "psimedia.h"
#include "ui_config.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "xmpp_xmlcommon.h"
#include "psiaccount.h"
#include "iris/ice176.h"

static QDomElement candidateToElement(QDomDocument *doc, const XMPP::Ice176::Candidate &c)
{
	QDomElement e = doc->createElement("candidate");
	e.setAttribute("component", QString::number(c.component));
	e.setAttribute("foundation", c.foundation);
	e.setAttribute("generation", QString::number(c.generation));
	if(!c.id.isEmpty())
		e.setAttribute("id", c.id);
	e.setAttribute("ip", c.ip.toString());
	e.setAttribute("network", QString::number(c.network));
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

/*class JT_PushJingleRtp : public Task
{
	Q_OBJECT

public:
	Jid from;
	QString from_id;

	QString peer_ipAddr;
	int peer_portA;
	int peer_portV;
	QString peer_config;

	JT_PushJingleRtp(Task *parent) :
		Task(parent)
	{
	}

	~JT_PushJingleRtp()
	{
	}

	void respondSuccess(const Jid &to, const QString &id, const QString &ipAddr, int portA, int portV, const QString &config)
	{
		QDomElement iq = createIQ(doc(), "result", to.full(), id);
		QDomElement query = doc()->createElement("request");
		query.setAttribute("xmlns", "http://barracuda.com/xjingle");
		query.appendChild(textTag(doc(), "address", ipAddr));
		query.appendChild(textTag(doc(), "audioBasePort", QString::number(portA)));
		query.appendChild(textTag(doc(), "videoBasePort", QString::number(portV)));
		query.appendChild(textTag(doc(), "config", config));
		iq.appendChild(query);
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

		QDomNodeList nl = e.elementsByTagName("request");
		QDomElement r;
		if(nl.count() > 0)
			r = nl.item(0).toElement();
		if(!r.isNull())
		{
			from = e.attribute("from");
			from_id = e.attribute("id");

			QDomElement i;

			i = r.elementsByTagName("address").item(0).toElement();
			peer_ipAddr = i.text();
			i = r.elementsByTagName("audioBasePort").item(0).toElement();
			peer_portA = i.text().toInt();
			i = r.elementsByTagName("videoBasePort").item(0).toElement();
			peer_portV = i.text().toInt();
			i = r.elementsByTagName("config").item(0).toElement();
			peer_config = i.text();

			emit incomingRequest();
			return true;
		}

		nl = e.elementsByTagName("terminate");
		r = QDomElement();
		if(nl.count() > 0)
			r = nl.item(0).toElement();
		if(!r.isNull())
		{
			from = e.attribute("from");
			from_id = e.attribute("id");

			emit incomingTerminate();
			return true;
		}

		return true;
	}

signals:
	void incomingRequest();
	void incomingTerminate();
};*/

//----------------------------------------------------------------------------
// JingleRtpSession
//----------------------------------------------------------------------------
#if 0
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

	void connectToJid(const Jid &jid, const QString &config);
	void accept();
	void reject_in();
	void reject_out();

private slots:
	void task_finished();
	void push_task_incomingRequest();
	void push_task_incomingTerminate();
};

class JingleRtpSessionPrivate : public QObject
{
	Q_OBJECT

public:
	JingleRtpSession *q;

	bool incoming;
	JingleRtpManager *manager;
	int selfBasePort;
	QHostAddress target;
	int audioBasePort, videoBasePort;
	QString receive_config;
	XMPP::Jid jid;
	PsiMedia::Producer producer;
	PsiMedia::Receiver receiver;
	bool transmitAudio, transmitVideo, transmitting;
	bool receiveAudio, receiveVideo;
	RtpBinding *sendAudioRtp, *sendVideoRtp;
	RtpBinding *receiveAudioRtp, *receiveVideoRtp;

	JingleRtpSessionPrivate(JingleRtpSession *_q) :
		QObject(_q),
		q(_q),
		producer(this),
		receiver(this),
		sendAudioRtp(0),
		sendVideoRtp(0),
		receiveAudioRtp(0),
		receiveVideoRtp(0)
	{
		//producer.setVideoWidget(ui.vw_self);
		//receiver.setVideoWidget(ui.vw_remote);

		connect(&producer, SIGNAL(started()), SLOT(producer_started()));
		connect(&producer, SIGNAL(stopped()), SLOT(producer_stopped()));
		connect(&producer, SIGNAL(error()), SLOT(producer_error()));
		connect(&receiver, SIGNAL(started()), SLOT(receiver_started()));
		connect(&receiver, SIGNAL(stopped()), SLOT(receiver_stopped()));
		connect(&receiver, SIGNAL(error()), SLOT(receiver_error()));
	}

	~JingleRtpSessionPrivate()
	{
		printf("~JingleRtpSessionPrivate\n");
		producer.stop();
		receiver.stop();

		cleanup_send_rtp();
		cleanup_receive_rtp();
	}

	static QString producerErrorToString(PsiMedia::Producer::Error e)
	{
		QString str;
		switch(e)
		{
			default: // generic
				str = tr("Generic error"); break;
		}
		return str;
	}

	static QString receiverErrorToString(PsiMedia::Receiver::Error e)
	{
		QString str;
		switch(e)
		{
			case PsiMedia::Receiver::ErrorSystem:
				str = tr("System error"); break;
			case PsiMedia::Receiver::ErrorCodec:
				str = tr("Codec error"); break;
			default: // generic
				str = tr("Generic error"); break;
		}
		return str;
	}

	void cleanup_send_rtp()
	{
		delete sendAudioRtp;
		sendAudioRtp = 0;
		delete sendVideoRtp;
		sendVideoRtp = 0;
	}

	void cleanup_receive_rtp()
	{
		delete receiveAudioRtp;
		receiveAudioRtp = 0;
		delete receiveVideoRtp;
		receiveVideoRtp = 0;
	}

public slots:
	void start_send()
	{
		manager->d->config = adjustConfiguration(manager->d->config, PsiMediaFeaturesSnapshot());
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

			if(!config.videoInDeviceId.isEmpty())
			{
				producer.setVideoInputDevice(config.videoInDeviceId);
				transmitVideo = true;
			}
		}
		else // non-live (file) input
		{
			producer.setFileInput(config.file);

			// we just assume the file has both audio and video.
			//   if it doesn't, no big deal, it'll still work.
			transmitAudio = true;
			transmitVideo = true;
		}

		if(transmitAudio)
		{
			QList<PsiMedia::AudioParams> audioParamsList;
			audioParamsList += config.audioParams;
			producer.setAudioParams(audioParamsList);
		}

		if(transmitVideo)
		{
			QList<PsiMedia::VideoParams> videoParamsList;
			videoParamsList += config.videoParams;
			producer.setVideoParams(videoParamsList);
		}

		//ui.pb_startSend->setEnabled(false);
		//ui.pb_stopSend->setEnabled(true);
		transmitting = false;
		producer.start();
	}

	void transmit()
	{
		QHostAddress addr = target;
		/*if(!addr.setAddress(ui.le_remoteAddress->text()))
		{
			QMessageBox::critical(this, tr("Error"), tr(
				"Invalid send IP address."
				));
			return;
		}*/

		int audioPort = -1;
		if(transmitAudio)
		{
			audioPort = audioBasePort;
			printf("sending audio to %s:%d\n", qPrintable(addr.toString()), audioPort);
			/*bool ok;
			audioPort = ui.le_remoteAudioPort->text().toInt(&ok);
			if(!ok || audioPort < BASE_PORT_MIN || audioPort > BASE_PORT_MAX)
			{
				QMessageBox::critical(this, tr("Error"), tr(
					"Invalid send audio port."
					));
				return;
			}*/
		}

		int videoPort = -1;
		if(transmitVideo)
		{
			videoPort = videoBasePort;
			printf("sending video to %s:%d\n", qPrintable(addr.toString()), videoPort);
			/*bool ok;
			videoPort = ui.le_remoteVideoPort->text().toInt(&ok);
			if(!ok || videoPort < BASE_PORT_MIN || videoPort > BASE_PORT_MAX)
			{
				QMessageBox::critical(this, tr("Error"), tr(
					"Invalid send video port."
					));
				return;
			}*/
		}

		RtpSocketGroup *audioSocketGroup = new RtpSocketGroup;
		sendAudioRtp = new RtpBinding(RtpBinding::Send, producer.audioRtpChannel(), audioSocketGroup, this);
		sendAudioRtp->sendAddress = addr;
		sendAudioRtp->sendBasePort = audioPort;

		RtpSocketGroup *videoSocketGroup = new RtpSocketGroup;
		sendVideoRtp = new RtpBinding(RtpBinding::Send, producer.videoRtpChannel(), videoSocketGroup, this);
		sendVideoRtp->sendAddress = addr;
		sendVideoRtp->sendBasePort = videoPort;

		//setSendFieldsEnabled(false);
		//ui.pb_transmit->setEnabled(false);

		if(transmitAudio)
			producer.transmitAudio();
		if(transmitVideo)
			producer.transmitVideo();

		transmitting = true;
	}

	void stop_send()
	{
		//ui.pb_stopSend->setEnabled(false);

		//if(!transmitting)
		//	ui.pb_transmit->setEnabled(false);

		producer.stop();
	}

	void start_receive()
	{
		//config = adjustConfiguration(config, PsiMediaFeaturesSnapshot());
		Configuration &config = manager->d->config;

		QString receiveConfig = receive_config; //ui.le_receiveConfig->text();
		PsiMedia::PayloadInfo audio;
		PsiMedia::PayloadInfo video;
		if(receiveConfig.isEmpty() || !codecStringToPayloadInfo(receiveConfig, &audio, &video))
		{
			QMessageBox::critical(0, tr("Error"), tr(
				"Invalid codec config."
				));
			return;
		}

		receiveAudio = !audio.isNull();
		receiveVideo = !video.isNull();

		int audioPort = -1;
		if(receiveAudio)
		{
			audioPort = selfBasePort;
			printf("listening for audio: %d\n", audioPort);
			/*bool ok;
			audioPort = ui.le_localAudioPort->text().toInt(&ok);
			if(!ok || audioPort < BASE_PORT_MIN || audioPort > BASE_PORT_MAX)
			{
				QMessageBox::critical(this, tr("Error"), tr(
					"Invalid receive audio port."
					));
				return;
			}*/
		}

		int videoPort = -1;
		if(receiveVideo)
		{
			videoPort = selfBasePort + 2;
			printf("listening for video: %d\n", videoPort);
			/*bool ok;
			videoPort = ui.le_localVideoPort->text().toInt(&ok);
			if(!ok || videoPort < BASE_PORT_MIN || videoPort > BASE_PORT_MAX)
			{
				QMessageBox::critical(this, tr("Error"), tr(
					"Invalid receive video port."
					));
				return;
			}*/
		}

		if(receiveAudio && !config.audioOutDeviceId.isEmpty())
		{
			receiver.setAudioOutputDevice(config.audioOutDeviceId);

			QList<PsiMedia::AudioParams> audioParamsList;
			audioParamsList += config.audioParams;
			receiver.setAudioParams(audioParamsList);

			QList<PsiMedia::PayloadInfo> payloadInfoList;
			payloadInfoList += audio;
			receiver.setAudioPayloadInfo(payloadInfoList);
		}

		if(receiveVideo)
		{
			QList<PsiMedia::VideoParams> videoParamsList;
			videoParamsList += config.videoParams;
			receiver.setVideoParams(videoParamsList);

			QList<PsiMedia::PayloadInfo> payloadInfoList;
			payloadInfoList += video;
			receiver.setVideoPayloadInfo(payloadInfoList);
		}

		RtpSocketGroup *audioSocketGroup = new RtpSocketGroup(this);
		RtpSocketGroup *videoSocketGroup = new RtpSocketGroup(this);
		if(receiveAudio && !audioSocketGroup->bind(audioPort))
		{
			delete audioSocketGroup;
			audioSocketGroup = 0;
			delete videoSocketGroup;
			videoSocketGroup = 0;

			QMessageBox::critical(0, tr("Error"), tr(
				"Unable to bind to receive audio ports."
				));
			return;
		}
		if(receiveVideo && !videoSocketGroup->bind(videoPort))
		{
			delete audioSocketGroup;
			audioSocketGroup = 0;
			delete videoSocketGroup;
			videoSocketGroup = 0;

			QMessageBox::critical(0, tr("Error"), tr(
				"Unable to bind to receive video ports."
				));
			return;
		}

		receiveAudioRtp = new RtpBinding(RtpBinding::Receive, receiver.audioRtpChannel(), audioSocketGroup, this);
		receiveVideoRtp = new RtpBinding(RtpBinding::Receive, receiver.videoRtpChannel(), videoSocketGroup, this);

		//setReceiveFieldsEnabled(false);
		//ui.pb_startReceive->setEnabled(false);
		//ui.pb_stopReceive->setEnabled(true);
		receiver.start();
	}

	void stop_receive()
	{
		//ui.pb_stopReceive->setEnabled(false);
		receiver.stop();
	}

	void change_volume_mic(int value)
	{
		producer.setVolume(value);
	}

	void change_volume_spk(int value)
	{
		receiver.setVolume(value);
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
			audio = producer.audioPayloadInfo().first();
			pAudio = &audio;
		}
		if(transmitVideo)
		{
			video = producer.videoPayloadInfo().first();
			pVideo = &video;
		}

		QString str = payloadInfoToCodecString(pAudio, pVideo);
		//setSendConfig(str);

		//ui.pb_transmit->setEnabled(true);

		// outbound?
		if(!incoming)
		{
			// start the request
			manager->d->connectToJid(jid, str);
		}
		// inbound?
		else
		{
			// respond to the request
			manager->d->push_task->respondSuccess(jid, manager->d->push_task->from_id, manager->d->self_addr.toString(), selfBasePort, selfBasePort + 2, str);

			// begin transmitting
			QTimer::singleShot(100, this, SLOT(transmit()));
			QTimer::singleShot(1000, this, SLOT(start_receive()));
		}
	}

	void producer_stopped()
	{
		cleanup_send_rtp();

		//setSendFieldsEnabled(true);
		//setSendConfig(QString());
		//ui.pb_startSend->setEnabled(true);
	}

	void producer_error()
	{
		cleanup_send_rtp();

		//setSendFieldsEnabled(true);
		//setSendConfig(QString());
		//ui.pb_startSend->setEnabled(true);
		//ui.pb_transmit->setEnabled(false);
		//ui.pb_stopSend->setEnabled(false);

		QMessageBox::critical(0, tr("Error"), tr(
			"An error occurred while trying to send:\n%1."
			).arg(producerErrorToString(producer.errorCode())
			));
	}

	void receiver_started()
	{
		//ui.pb_record->setEnabled(true);

		emit q->activated();
	}

	void receiver_stopped()
	{
		cleanup_receive_rtp();
		//cleanup_record();

		//setReceiveFieldsEnabled(true);
		//ui.pb_startReceive->setEnabled(true);
		//ui.pb_record->setEnabled(false);
	}

	void receiver_error()
	{
		cleanup_receive_rtp();
		//cleanup_record();

		//setReceiveFieldsEnabled(true);
		//ui.pb_startReceive->setEnabled(true);
		//ui.pb_stopReceive->setEnabled(false);
		//ui.pb_record->setEnabled(false);

		QMessageBox::critical(0, tr("Error"), tr(
			"An error occurred while trying to receive:\n%1."
			).arg(receiverErrorToString(receiver.errorCode())
			));
	}
};
#endif

JingleRtpSession::JingleRtpSession() :
	QObject(0)
{
	//d = new JingleRtpSessionPrivate(this);
}

JingleRtpSession::~JingleRtpSession()
{
	/*if(d->incoming)
		d->manager->d->sess_in = 0;
	else
		d->manager->d->sess_out = 0;
	delete d;*/
}

XMPP::Jid JingleRtpSession::jid() const
{
	return Jid();
	//return d->jid;
}

void JingleRtpSession::connectToJid(const XMPP::Jid &jid)
{
	Q_UNUSED(jid);
	//d->jid = jid;
	//d->start_send();
}

void JingleRtpSession::accept()
{
	//d->manager->d->accept();
}

void JingleRtpSession::reject()
{
	/*if(d->incoming)
		d->manager->d->reject_in();
	else
		d->manager->d->reject_out();*/
}

void JingleRtpSession::setIncomingVideo(PsiMedia::VideoWidget *widget)
{
	Q_UNUSED(widget);
	//d->receiver.setVideoWidget(widget);
}

//----------------------------------------------------------------------------
// JingleRtpManager
//----------------------------------------------------------------------------
/*static JingleRtpManager *g_manager = 0;

JingleRtpManagerPrivate::JingleRtpManagerPrivate(PsiAccount *_pa, JingleRtpManager *_q) :
	QObject(_q),
	q(_q),
	pa(_pa)
{
#ifndef GSTPROVIDER_STATIC
# ifdef GSTBUNDLE_PATH
	PsiMedia::loadPlugin("gstprovider", GSTBUNDLE_PATH);
# else
	PsiMedia::loadPlugin("gstprovider", QString());
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

	connect(push_task, SIGNAL(incomingRequest()), SLOT(push_task_incomingRequest()));
	connect(push_task, SIGNAL(incomingTerminate()), SLOT(push_task_incomingTerminate()));
}

JingleRtpManagerPrivate::~JingleRtpManagerPrivate()
{
}

void JingleRtpManagerPrivate::connectToJid(const Jid &jid, const QString &config)
{
	task = new JT_JingleRtp(pa->client()->rootTask());
	connect(task, SIGNAL(finished()), SLOT(task_finished()));
	task->request(jid, self_addr.toString(), sess_out->d->selfBasePort, sess_out->d->selfBasePort + 2, config);
	task->go(true);
}

void JingleRtpManagerPrivate::accept()
{
	sess_in->d->start_send();
}

void JingleRtpManagerPrivate::reject_in()
{
	// reject incoming request, if any
	push_task->respondError(push_task->from, push_task->from_id, 400, "reject");

	// terminate active session, if any
	JT_JingleRtp *jt = new JT_JingleRtp(pa->client()->rootTask());
	jt->terminate(sess_in->d->jid);
	jt->go(true);
}

void JingleRtpManagerPrivate::reject_out()
{
	// terminate active session, if any
	JT_JingleRtp *jt = new JT_JingleRtp(pa->client()->rootTask());
	jt->terminate(sess_out->d->jid);
	jt->go(true);
}

void JingleRtpManagerPrivate::task_finished()
{
	JT_JingleRtp *jt = task;
	task = 0;

	if(!sess_out)
		return;

	if(jt->success())
	{
		sess_out->d->target = jt->peer_ipAddr;
		sess_out->d->audioBasePort = jt->peer_portA;
		sess_out->d->videoBasePort = jt->peer_portV;
		sess_out->d->receive_config = jt->peer_config;
		sess_out->d->transmit();
		QTimer::singleShot(1000, sess_out->d, SLOT(start_receive()));
	}
	else
		emit sess_out->rejected();
}

void JingleRtpManagerPrivate::push_task_incomingRequest()
{
	sess_in = new JingleRtpSession;
	sess_in->d->manager = q;
	sess_in->d->incoming = true;
	sess_in->d->selfBasePort = 62000;
	sess_in->d->jid = push_task->from;
	sess_in->d->target = push_task->peer_ipAddr;
	sess_in->d->audioBasePort = push_task->peer_portA;
	sess_in->d->videoBasePort = push_task->peer_portV;
	sess_in->d->receive_config = push_task->peer_config;
	emit q->incomingReady();
}

void JingleRtpManagerPrivate::push_task_incomingTerminate()
{
	//if(sess_in)
	//	emit sess_in->rejected();
	//else if(sess_out)
	//	emit sess_out->rejected();
}*/

JingleRtpManager::JingleRtpManager(PsiAccount *pa) :
	QObject(0)
{
	Q_UNUSED(pa);
	//g_manager = this;
	//d = new JingleRtpManagerPrivate(pa, this);
}

JingleRtpManager::~JingleRtpManager()
{
	/*delete d->push_task;
	delete d;
	g_manager = 0;*/
}

JingleRtpManager *JingleRtpManager::instance()
{
	return 0;
	//return g_manager;
}

JingleRtpSession *JingleRtpManager::createOutgoing()
{
	JingleRtpSession *sess = new JingleRtpSession;
	/*d->sess_out = sess;
	sess->d->manager = this;
	sess->d->incoming = false;
	sess->d->selfBasePort = 60000;*/
	return sess;
}

JingleRtpSession *JingleRtpManager::takeIncoming()
{
	return 0;
	//return d->sess_in;
}

void JingleRtpManager::config()
{
	/*ConfigDlg w(d->config, 0);
	w.exec();
	d->config = w.config;*/
}

void JingleRtpManager::setSelfAddress(const QHostAddress &addr)
{
	Q_UNUSED(addr);
	//d->self_addr = addr;
}

#ifdef GSTPROVIDER_STATIC
Q_IMPORT_PLUGIN(gstprovider)
#endif

#include "jinglertp.moc"
