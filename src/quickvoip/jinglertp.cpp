#include "jinglertp.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include "../psimedia/psimedia.h"
#include "ui_config.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "xmpp_xmlcommon.h"
#include "psiaccount.h"

#define BASE_PORT_MIN 1
#define BASE_PORT_MAX 65534

static QString urlishEncode(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '%' || in[n] == ';' || in[n] == ':' || in[n] == '\n')
		{
			unsigned char c = (unsigned char)in[n].toLatin1();
			out += QString().sprintf("%%%02x", c);
		}
		else
			out += in[n];
	}
	return out;
}

static QString urlishDecode(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '%')
		{
			if(n + 2 >= in.length())
				return QString();

			QString hex = in.mid(n + 1, 2);
			bool ok;
			int x = hex.toInt(&ok, 16);
			if(!ok)
				return QString();

			unsigned char c = (unsigned char)x;
			out += c;
			n += 2;
		}
		else
			out += in[n];
	}
	return out;
}

static QString payloadInfoToString(const PsiMedia::PayloadInfo &info)
{
	QStringList list;
	list += QString::number(info.id());
	list += info.name();
	list += QString::number(info.clockrate());
	list += QString::number(info.channels());
	list += QString::number(info.ptime());
	list += QString::number(info.maxptime());
	foreach(const PsiMedia::PayloadInfo::Parameter &p, info.parameters())
		list += p.name + '=' + p.value;

	for(int n = 0; n < list.count(); ++n)
		list[n] = urlishEncode(list[n]);
	return list.join(",");
}

static PsiMedia::PayloadInfo stringToPayloadInfo(const QString &in)
{
	QStringList list = in.split(',');
	if(list.count() < 6)
		return PsiMedia::PayloadInfo();

	for(int n = 0; n < list.count(); ++n)
	{
		QString str = urlishDecode(list[n]);
		if(str.isEmpty())
			return PsiMedia::PayloadInfo();
		list[n] = str;
	}

	PsiMedia::PayloadInfo out;
	bool ok;
	int x;

	x = list[0].toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setId(x);

	out.setName(list[1]);

	x = list[2].toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setClockrate(x);

	x = list[3].toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setChannels(x);

	x = list[4].toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setPtime(x);

	x = list[5].toInt(&ok);
	if(!ok)
		return PsiMedia::PayloadInfo();
	out.setMaxptime(x);

	QList<PsiMedia::PayloadInfo::Parameter> plist;
	for(int n = 6; n < list.count(); ++n)
	{
		x = list[n].indexOf('=');
		if(x == -1)
			return PsiMedia::PayloadInfo();
		PsiMedia::PayloadInfo::Parameter p;
		p.name = list[n].mid(0, x);
		p.value = list[n].mid(x + 1);
		plist += p;
	}
	out.setParameters(plist);

	return out;
}

static QString payloadInfoToCodecString(const PsiMedia::PayloadInfo *audio, const PsiMedia::PayloadInfo *video)
{
	QStringList list;
	if(audio)
		list += QString("A:") + payloadInfoToString(*audio);
	if(video)
		list += QString("V:") + payloadInfoToString(*video);
	return list.join(";");
}

static bool codecStringToPayloadInfo(const QString &in, PsiMedia::PayloadInfo *audio, PsiMedia::PayloadInfo *video)
{
	QStringList list = in.split(';');
	foreach(const QString &s, list)
	{
		int x = s.indexOf(':');
		if(x == -1)
			return false;

		QString var = s.mid(0, x);
		QString val = s.mid(x + 1);
		if(val.isEmpty())
			return false;

		PsiMedia::PayloadInfo info = stringToPayloadInfo(val);
		if(info.isNull())
			return false;

		if(var == "A" && audio)
			*audio = info;
		else if(var == "V" && video)
			*video = info;
	}

	return true;
}

Q_DECLARE_METATYPE(PsiMedia::AudioParams);
Q_DECLARE_METATYPE(PsiMedia::VideoParams);

class Configuration
{
public:
	bool liveInput;
	QString audioOutDeviceId, audioInDeviceId, videoInDeviceId;
	QString file;
	PsiMedia::AudioParams audioParams;
	PsiMedia::VideoParams videoParams;

	Configuration() :
		liveInput(false)
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
		audioOutputDevices = PsiMedia::audioOutputDevices();
		audioInputDevices = PsiMedia::audioInputDevices();
		videoInputDevices = PsiMedia::videoInputDevices();
		supportedAudioModes = PsiMedia::supportedAudioModes();
		supportedVideoModes = PsiMedia::supportedVideoModes();
	}
};

// get default settings
static Configuration getDefaultConfiguration()
{
	Configuration config;
	config.liveInput = true;

	QList<PsiMedia::Device> devs;

	devs = PsiMedia::audioOutputDevices();
	if(!devs.isEmpty())
		config.audioOutDeviceId = devs.first().id();

	devs = PsiMedia::audioInputDevices();
	if(!devs.isEmpty())
		config.audioInDeviceId = devs.first().id();

	devs = PsiMedia::videoInputDevices();
	if(!devs.isEmpty())
		config.videoInDeviceId = devs.first().id();

	config.audioParams = PsiMedia::supportedAudioModes().first();
	config.videoParams = PsiMedia::supportedVideoModes().first();

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
			codec[0] = codec[0].toUpper();
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
			codec[0] = codec[0].toUpper();
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
	}

	void file_choose()
	{
		QString fileName = QFileDialog::getOpenFileName(this,
			tr("Open File"),
			QDir::homePath(),
			tr("Ogg Audio/Video (*.ogg)"));
		if(!fileName.isEmpty())
			ui.le_file->setText(fileName);
	}
};

// handles two udp sockets
class RtpSocketGroup : public QObject
{
	Q_OBJECT

public:
	QUdpSocket socket[2];

	RtpSocketGroup(QObject *parent = 0) :
		QObject(parent)
	{
		connect(&socket[0], SIGNAL(readyRead()), SLOT(sock_readyRead()));
		connect(&socket[1], SIGNAL(readyRead()), SLOT(sock_readyRead()));
		connect(&socket[0], SIGNAL(bytesWritten(qint64)), SLOT(sock_bytesWritten(qint64)));
		connect(&socket[1], SIGNAL(bytesWritten(qint64)), SLOT(sock_bytesWritten(qint64)));
	}

	bool bind(int basePort)
	{
		if(!socket[0].bind(basePort))
			return false;
		if(!socket[1].bind(basePort + 1))
			return false;
		return true;
	}

signals:
	void readyRead(int offset);
	void datagramWritten(int offset);

private slots:
	void sock_readyRead()
	{
		QUdpSocket *udp = (QUdpSocket *)sender();
		if(udp == &socket[0])
			emit readyRead(0);
		else
			emit readyRead(1);
	}

	void sock_bytesWritten(qint64 bytes)
	{
		Q_UNUSED(bytes);

		QUdpSocket *udp = (QUdpSocket *)sender();
		if(udp == &socket[0])
			emit datagramWritten(0);
		else
			emit datagramWritten(1);
	}
};

// bind a channel to a socket group.
// takes ownership of socket group.
class RtpBinding : public QObject
{
	Q_OBJECT

public:
	enum Mode
	{
		Send,
		Receive
	};

	Mode mode;
	PsiMedia::RtpChannel *channel;
	RtpSocketGroup *socketGroup;
	QHostAddress sendAddress;
	int sendBasePort;

	RtpBinding(Mode _mode, PsiMedia::RtpChannel *_channel, RtpSocketGroup *_socketGroup, QObject *parent = 0) :
		QObject(parent),
		mode(_mode),
		channel(_channel),
		socketGroup(_socketGroup),
		sendBasePort(-1)
	{
		socketGroup->setParent(this);
		connect(socketGroup, SIGNAL(readyRead(int)), SLOT(net_ready(int)));
		connect(socketGroup, SIGNAL(datagramWritten(int)), SLOT(net_written(int)));
		connect(channel, SIGNAL(readyRead()), SLOT(app_ready()));
		connect(channel, SIGNAL(packetsWritten(int)), SLOT(app_written(int)));
	}

private slots:
	void net_ready(int offset)
	{
		// here we handle packets received from the network, that
		//   we need to give to psimedia

		while(socketGroup->socket[offset].hasPendingDatagrams())
		{
			int size = (int)socketGroup->socket[offset].pendingDatagramSize();
			QByteArray rawValue(size, offset);
			QHostAddress fromAddr;
			quint16 fromPort;
			if(socketGroup->socket[offset].readDatagram(rawValue.data(), size, &fromAddr, &fromPort) == -1)
				continue;

			// if we are sending RTP, we should not be receiving
			//   anything on offset 0
			if(mode == Send && offset == 0)
				continue;

			PsiMedia::RtpPacket packet(rawValue, offset);
			channel->write(packet);
		}
	}

	void net_written(int offset)
	{
		Q_UNUSED(offset);
		// do nothing
	}

	void app_ready()
	{
		// here we handle packets that psimedia wants to send out,
		//   that we need to give to the network

		while(channel->packetsAvailable() > 0)
		{
			PsiMedia::RtpPacket packet = channel->read();
			int offset = packet.portOffset();
			if(offset < 0 || offset > 1)
				continue;

			// if we are receiving RTP, we should not be sending
			//   anything on offset 0
			if(mode == Receive && offset == 0)
				continue;

			if(sendAddress.isNull() || sendBasePort < BASE_PORT_MIN || sendBasePort > BASE_PORT_MAX)
				continue;

			socketGroup->socket[offset].writeDatagram(packet.rawValue(), sendAddress, sendBasePort + offset);
		}
	}

	void app_written(int count)
	{
		Q_UNUSED(count);
		// do nothing
	}
};

using namespace XMPP;

class JT_JingleRtp : public Task
{
	Q_OBJECT
public:
	QDomElement iq;
	Jid to;

	QString peer_ipAddr;
	int peer_portA;
	int peer_portV;
	QString peer_config;

	JT_JingleRtp(Task *parent) :
		Task(parent)
	{
	}

	~JT_JingleRtp()
	{
	}

	void request(const Jid &_to, const QString &ipAddr, int portA, int portV, const QString &config)
	{
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("request");
		query.setAttribute("xmlns", "http://barracuda.com/xjingle");
		query.appendChild(textTag(doc(), "address", ipAddr));
		query.appendChild(textTag(doc(), "audioBasePort", QString::number(portA)));
		query.appendChild(textTag(doc(), "videoBasePort", QString::number(portV)));
		query.appendChild(textTag(doc(), "config", config));
		iq.appendChild(query);
	}

	void terminate(const Jid &_to)
	{
		to = _to;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("terminate");
		query.setAttribute("xmlns", "http://barracuda.com/xjingle");
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
		{
			QDomElement r = x.elementsByTagName("request").item(0).toElement();
			if(!r.isNull())
			{
				QDomElement e;

				e = r.elementsByTagName("address").item(0).toElement();
				peer_ipAddr = e.text();
				e = r.elementsByTagName("audioBasePort").item(0).toElement();
				peer_portA = e.text().toInt();
				e = r.elementsByTagName("videoBasePort").item(0).toElement();
				peer_portV = e.text().toInt();
				e = r.elementsByTagName("config").item(0).toElement();
				peer_config = e.text();

				setSuccess();
			}
		}
		else
		{
			setError(x);
		}

		return true;
	}
};

class JT_PushJingleRtp : public Task
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
	d->receiver.setVideoWidget(widget);
}

//----------------------------------------------------------------------------
// JingleRtpManager
//----------------------------------------------------------------------------
static JingleRtpManager *g_manager = 0;

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
}

JingleRtpManager::JingleRtpManager(PsiAccount *pa) :
	QObject(0)
{
	g_manager = this;
	d = new JingleRtpManagerPrivate(pa, this);
}

JingleRtpManager::~JingleRtpManager()
{
	delete d->push_task;
	delete d;
	g_manager = 0;
}

JingleRtpManager *JingleRtpManager::instance()
{
	return g_manager;
}

JingleRtpSession *JingleRtpManager::createOutgoing()
{
	JingleRtpSession *sess = new JingleRtpSession;
	d->sess_out = sess;
	sess->d->manager = this;
	sess->d->incoming = false;
	sess->d->selfBasePort = 60000;
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

#ifdef GSTPROVIDER_STATIC
Q_IMPORT_PLUGIN(gstprovider)
#endif

#include "jinglertp.moc"
