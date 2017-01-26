/*
 * httpuploadplugin.cpp - plugin
 * Copyright (C) 2016  rkfg
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "psiplugin.h"
#include "activetabaccessinghost.h"
#include "activetabaccessor.h"
#include "toolbariconaccessor.h"
#include "iconfactoryaccessor.h"
#include "iconfactoryaccessinghost.h"
#include "gctoolbariconaccessor.h"
#include "stanzasender.h"
#include "stanzasendinghost.h"
#include "accountinfoaccessinghost.h"
#include "accountinfoaccessor.h"
#include "plugininfoprovider.h"
#include "psiaccountcontroller.h"
#include "psiaccountcontrollinghost.h"
#include "optionaccessinghost.h"
#include "optionaccessor.h"
#include "applicationinfoaccessor.h"
#include "applicationinfoaccessinghost.h"
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QTextDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include "chattabaccessor.h"
#include "stanzafilter.h"
#include <QDomElement>
#include "uploadservice.h"
#include "currentupload.h"
#include <QCheckBox>
#include <QSpinBox>
#include "previewfiledialog.h"

//#define DEBUG_UPLOAD
#define constVersion "0.1.0"
#define CONST_LAST_FOLDER "httpupload-lastfolder"
#define SLOT_TIMEOUT 10000
#define OPTION_RESIZE "httpupload-do-resize"
#define OPTION_SIZE "httpupload-image-size"
#define OPTION_QUALITY "httpupload-image-quality"
#define OPTION_PREVIEW_WIDTH "httpupload-preview-width"

QString escape(const QString &plain) {
#ifdef HAVE_QT5
	return plain.toHtmlEscaped();
#else
	return Qt::escape(plain);
#endif
}

class HttpUploadPlugin: public QObject,
		public PsiPlugin,
		public ToolbarIconAccessor,
		public GCToolbarIconAccessor,
		public StanzaSender,
		public IconFactoryAccessor,
		public ActiveTabAccessor,
		public PluginInfoProvider,
		public AccountInfoAccessor,
		public PsiAccountController,
		public OptionAccessor,
		public ChatTabAccessor,
		public StanzaFilter,
		public ApplicationInfoAccessor {
Q_OBJECT
#ifdef HAVE_QT5
Q_PLUGIN_METADATA(IID "com.psi-plus.HttpUploadPlugin")
#endif
Q_INTERFACES(PsiPlugin ToolbarIconAccessor GCToolbarIconAccessor
		StanzaSender ActiveTabAccessor PsiAccountController OptionAccessor
		IconFactoryAccessor AccountInfoAccessor PluginInfoProvider ChatTabAccessor
		StanzaFilter ApplicationInfoAccessor)
public:
	HttpUploadPlugin();
	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options();
	virtual bool enable();
	virtual bool disable();

	virtual void applyOptions();
	virtual void restoreOptions();
	virtual QList<QVariantHash> getButtonParam();
	virtual QAction* getAction(QObject*, int, const QString&) {
		return 0;
	}
	virtual QList<QVariantHash> getGCButtonParam();
	virtual QAction* getGCAction(QObject*, int, const QString&) {
		return 0;
	}
	virtual void setIconFactoryAccessingHost(IconFactoryAccessingHost* host);
	virtual void setStanzaSendingHost(StanzaSendingHost *host);
	virtual void setActiveTabAccessingHost(ActiveTabAccessingHost* host);
	virtual void setAccountInfoAccessingHost(AccountInfoAccessingHost* host);
	virtual void setPsiAccountControllingHost(PsiAccountControllingHost *host);
	virtual void setOptionAccessingHost(OptionAccessingHost *host);
	virtual void optionChanged(const QString &) {
	}
	virtual QString pluginInfo();
	virtual QPixmap icon() const;
	virtual void setupChatTab(QWidget*, int account, const QString&) {
		checkUploadAvailability(account);
	}
	virtual void setupGCTab(QWidget*, int account, const QString&) {
		checkUploadAvailability(account);
	}

	virtual bool appendingChatMessage(int, const QString&, QString&, QDomElement&, bool) {
		return false;
	}
	virtual bool incomingStanza(int account, const QDomElement& xml);
	virtual bool outgoingStanza(int, QDomElement &) {
		return false;
	}
	QString getId(int account) {
		return stanzaSender->uniqueId(account);
	}
	void checkUploadAvailability(int account);
	void setApplicationInfoAccessingHost(ApplicationInfoAccessingHost* host);
	void updateProxy();

private slots:
	void uploadFile();
	void uploadImage();
	void uploadComplete(QNetworkReply* reply);
	void timeout();
	void resizeStateChanged(int state);

private:
	void upload(bool anything);
	int accountNumber() {
		QString jid = activeTab->getYourJid();
		QString jidToSend = activeTab->getJid();
		int account = 0;
		QString tmpJid("");
		while (jid != (tmpJid = accInfo->getJid(account))) {
			++account;
			if (tmpJid == "-1")
				return -1;
		}
		return account;
	}

	void cancelTimeout() {
		slotTimeout.stop();
		if (dataSource) {
			dataSource->deleteLater();
		}
		if (imageBytes) {
			delete imageBytes;
			imageBytes = 0;
		}
	}
	void processServices(const QDomElement& query, int account);
	void processOneService(const QDomElement& query, const QString& service, int account);
	void processUploadSlot(const QDomElement& xml);

	IconFactoryAccessingHost* iconHost;
	StanzaSendingHost* stanzaSender;
	ActiveTabAccessingHost* activeTab;
	AccountInfoAccessingHost* accInfo;
	PsiAccountControllingHost *psiController;
	OptionAccessingHost *psiOptions;
	ApplicationInfoAccessingHost* appInfoHost;
	bool enabled;
	QHash<QString, int> accounts_;
	QNetworkAccessManager* manager;
	QMap<QString, UploadService> serviceNames;
	QPointer<QIODevice> dataSource;
	QByteArray* imageBytes;
	CurrentUpload currentUpload;
	QTimer slotTimeout;
	QSpinBox *sb_previewWidth;
	QCheckBox *cb_resize;
	QSpinBox *sb_size;
	QSpinBox *sb_quality;
	bool imageResize;
	int imageSize;
	int imageQuality;
	int previewWidth;
};

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN(HttpUploadPlugin)
#endif

HttpUploadPlugin::HttpUploadPlugin() :
		iconHost(0), stanzaSender(0), activeTab(0), accInfo(0), psiController(0), psiOptions(0), appInfoHost(0), enabled(
				false), manager(new QNetworkAccessManager(this)), imageBytes(0), sb_previewWidth(0), cb_resize(0), sb_size(
				0), sb_quality(0), imageResize(false), imageSize(0), imageQuality(0), previewWidth(0) {
	connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(uploadComplete(QNetworkReply*)));
	connect(&slotTimeout, SIGNAL(timeout()), this, SLOT(timeout()));
	slotTimeout.setSingleShot(true);
}

QString HttpUploadPlugin::name() const {
	return "HTTP Upload Plugin";
}

QString HttpUploadPlugin::shortName() const {
	return "httpupload";
}
QString HttpUploadPlugin::version() const {
	return constVersion;
}

bool HttpUploadPlugin::enable() {
	QFile image_icon(":/httpuploadplugin/upload_image.png");
	QByteArray image;
	enabled = true;
	if (image_icon.open(QIODevice::ReadOnly)) {
		image = image_icon.readAll();
		iconHost->addIcon("httpuploadplugin/upload_image", image);
		image_icon.close();
	} else {
		enabled = false;
	}
	QFile file_icon(":/httpuploadplugin/upload_file.png");
	if (file_icon.open(QIODevice::ReadOnly)) {
		image = file_icon.readAll();
		iconHost->addIcon("httpuploadplugin/upload_file", image);
		file_icon.close();
	} else {
		enabled = false;
	}
	imageResize = psiOptions->getPluginOption(OPTION_RESIZE, false).toBool();
	imageSize = psiOptions->getPluginOption(OPTION_SIZE, 1024).toInt();
	imageQuality = psiOptions->getPluginOption(OPTION_QUALITY, 75).toInt();
	previewWidth = psiOptions->getPluginOption(OPTION_PREVIEW_WIDTH, 150).toInt();
	updateProxy();
	return enabled;
}

bool HttpUploadPlugin::disable() {
	enabled = false;
	return true;
}

QWidget* HttpUploadPlugin::options() {
	if (!enabled) {
		return 0;
	}
	QWidget *optionsWid = new QWidget();
	QVBoxLayout *vbox = new QVBoxLayout(optionsWid);
	vbox->addWidget(new QLabel(tr("Image preview width")));
	sb_previewWidth = new QSpinBox();
	sb_previewWidth->setMinimum(1);
	sb_previewWidth->setMaximum(65535);
	vbox->addWidget(sb_previewWidth);
	cb_resize = new QCheckBox(tr("Resize images"));
	vbox->addWidget(cb_resize);
	vbox->addWidget(new QLabel(tr("If width or height is bigger than")));
	sb_size = new QSpinBox();
	sb_size->setMinimum(1);
	sb_size->setMaximum(65535);
	sb_size->setEnabled(false);
	vbox->addWidget(sb_size);
	vbox->addWidget(new QLabel(tr("JPEG quality")));
	sb_quality = new QSpinBox();
	sb_quality->setMinimum(1);
	sb_quality->setMaximum(100);
	sb_quality->setEnabled(false);
	vbox->addWidget(sb_quality);
	vbox->addStretch();
	connect(cb_resize, SIGNAL(stateChanged(int)), this, SLOT(resizeStateChanged(int)));
	updateProxy();
	return optionsWid;
}

QList<QVariantHash> HttpUploadPlugin::getButtonParam() {
	QList<QVariantHash> l;
	QVariantHash uploadImg;
	uploadImg["tooltip"] = tr("Upload Image");
	uploadImg["icon"] = QString("httpuploadplugin/upload_image");
	uploadImg["reciver"] = qVariantFromValue(qobject_cast<QObject *>(this));
	uploadImg["slot"] = QVariant(SLOT(uploadImage()));
	l.push_back(uploadImg);
	QVariantHash uploadFile;
	uploadFile["tooltip"] = tr("Upload File");
	uploadFile["icon"] = QString("httpuploadplugin/upload_file");
	uploadFile["reciver"] = qVariantFromValue(qobject_cast<QObject *>(this));
	uploadFile["slot"] = QVariant(SLOT(uploadFile()));
	l.push_back(uploadFile);
	return l;
}

QList<QVariantHash> HttpUploadPlugin::getGCButtonParam() {
	return getButtonParam();
}

void HttpUploadPlugin::setAccountInfoAccessingHost(AccountInfoAccessingHost* host) {
	accInfo = host;
}

void HttpUploadPlugin::setIconFactoryAccessingHost(IconFactoryAccessingHost* host) {
	iconHost = host;
}

void HttpUploadPlugin::setPsiAccountControllingHost(PsiAccountControllingHost *host) {
	psiController = host;
}

void HttpUploadPlugin::setOptionAccessingHost(OptionAccessingHost *host) {
	psiOptions = host;
}

void HttpUploadPlugin::setStanzaSendingHost(StanzaSendingHost *host) {
	stanzaSender = host;
}

void HttpUploadPlugin::setActiveTabAccessingHost(ActiveTabAccessingHost* host) {
	activeTab = host;
}

void HttpUploadPlugin::uploadFile() {
	upload(true);
}

void HttpUploadPlugin::uploadImage() {
	upload(false);
}

void HttpUploadPlugin::upload(bool anything) {
	if (!enabled)
		return;
	if (dataSource) {
		QMessageBox::warning(0, tr("Please wait"),
				tr(
						"Another upload operation is already in progress. Please wait up to %1 sec for it to complete or fail.").arg(
				SLOT_TIMEOUT / 1000));
		return;
	}
	if (imageBytes) {
		delete imageBytes;
		imageBytes = 0;
	}
	QString serviceName;
	int sizeLimit = -1;
	int account = accountNumber();
	QString curJid = accInfo->getJid(account);
	QMap<QString, UploadService>::iterator iter = serviceNames.find(curJid);
	if (iter == serviceNames.end()) {
		QMessageBox::critical(0, tr("Not supported"),
				tr("Server for account %1 does not support HTTP Upload (XEP-363)").arg(curJid));
		return;
	}
	serviceName = iter->serviceName();
	sizeLimit = iter->sizeLimit();
	QString fileName;
	QString jid = activeTab->getYourJid();
	QString jidToSend = activeTab->getJid();

	if ("offline" == accInfo->getStatus(account)) {
		return;
	}

	QString imageName;
	const QString lastPath = psiOptions->getPluginOption(CONST_LAST_FOLDER, QDir::homePath()).toString();
	PreviewFileDialog dlg(0, anything ? tr("Upload file") : tr("Upload image"), lastPath,
			anything ? "" : tr("Images (*.png *.gif *.jpg *.jpeg)"), previewWidth);
	dlg.setAcceptMode(QFileDialog::AcceptOpen);
	if (!dlg.exec()) {
		return;
	}
	fileName = dlg.selectedFiles()[0];
	QFile file(fileName);
	QFileInfo fileInfo(file);
	QPixmap pix(fileName);
	imageName = fileInfo.fileName();
	psiOptions->setPluginOption(CONST_LAST_FOLDER, fileInfo.path());
	QString mimeType("application/octet-stream"); // proper MIME detection is possible with Qt5, for upload purposes this should be fine
	int length;
	QString lowerImagename = imageName.toLower();
	// only resize jpg and png
	if (!anything && imageResize
			&& (lowerImagename.endsWith(".jpg") || lowerImagename.endsWith(".jpeg") || lowerImagename.endsWith(".png"))
			&& (pix.width() > imageSize || pix.height() > imageSize)) {
		imageBytes = new QByteArray();
		dataSource = new QBuffer(imageBytes);
		QString type = "jpg";
		if (lowerImagename.endsWith(".png")) {
			type = "png";
		}
		pix.scaled(imageSize, imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(dataSource,
				type.toLatin1().constData(), imageQuality);
		length = imageBytes->length();
#ifdef DEBUG_UPLOAD
		qDebug() << "Resized length:" << length;
#endif
		dataSource->open(QIODevice::ReadOnly);
	} else {
		length = fileInfo.size();
		dataSource = new QFile(fileName, this);
		if (!dataSource->open(QIODevice::ReadOnly)) {
			dataSource->deleteLater();
			QMessageBox::critical(0, tr("Error"), tr("Error opening file %1").arg(fileName));
			return;
		}
	}
	if (length > sizeLimit) {
		QMessageBox::critical(0, tr("The file size is too large."),
				tr("File size must be less than %1 bytes").arg(sizeLimit));
		return;
	}
	currentUpload.account = account;
	currentUpload.from = jid;
	currentUpload.to = jidToSend;
	currentUpload.type =
			QLatin1String(sender()->parent()->metaObject()->className()) == "PsiChatDlg" ? "chat" : "groupchat";

	QString slotRequestStanza = QString("<iq from='%1' id='%2' to='%3' type='get'>"
			"<request xmlns='urn:xmpp:http:upload'>"
			"<filename>%4</filename>"
			"<size>%5</size>"
			"<content-type>%6</content-type>"
			"</request>"
			"</iq>").arg(jid).arg(getId(account)).arg(serviceName).arg(escape(imageName)).arg(length).arg(mimeType);
#ifdef DEBUG_UPLOAD
	qDebug() << "Requesting slot:" << slotRequestStanza;
#endif
	slotTimeout.start(SLOT_TIMEOUT);
	stanzaSender->sendStanza(account, slotRequestStanza);
}

QString HttpUploadPlugin::pluginInfo() {
	return tr("Authors: ") + "rkfg\n\n" + trUtf8("This plugin allows uploading images and other files via XEP-0363.");
}

QPixmap HttpUploadPlugin::icon() const {
	return QPixmap(":/httpuploadplugin/upload_image.png");
}

void HttpUploadPlugin::checkUploadAvailability(int account) {
	QString curJid = accInfo->getJid(account);
	if (serviceNames.find(curJid) != serviceNames.end()) {
		return;
	}
	QRegExp jidRE("^([^@]*)@([^/]*)$");
	if (jidRE.indexIn(curJid) == 0) {
		QString domain = jidRE.cap(2);
		QString id = getId(account);
		// send discovery request to the main domain for Prosody to work
		QString discoMain = QString("<iq xmlns='jabber:client' from='%1' id='%2' to='%3' type='get'>"
				"<query xmlns='http://jabber.org/protocol/disco#info'/>"
				"</iq>").arg(curJid).arg(id).arg(domain);
		stanzaSender->sendStanza(account, discoMain);
		QString disco = QString("<iq from='%1' id='%2' to='%3' type='get'>"
				"<query xmlns='http://jabber.org/protocol/disco#items'/>"
				"</iq>").arg(curJid).arg(id).arg(domain);
		stanzaSender->sendStanza(account, disco);
	}
}

void HttpUploadPlugin::processServices(const QDomElement& query, int account) {
	QString curJid = accInfo->getJid(account);
	QDomNodeList nodes = query.childNodes();
	for (int i = 0; i < nodes.count(); i++) {
		QDomElement elem = nodes.item(i).toElement();
		if (elem.tagName() == "item") {
			QString serviceJid = elem.attribute("jid");
			QString serviceDiscoStanza = QString("<iq from='%1' id='%2' to='%3' type='get'>"
					"<query xmlns='http://jabber.org/protocol/disco#info'/>"
					"</iq>").arg(curJid).arg(getId(account)).arg(serviceJid);
#ifdef DEBUG_UPLOAD
			qDebug() << "Discovering service" << serviceJid;
#endif
			stanzaSender->sendStanza(account, serviceDiscoStanza);
		}
	}
}

void HttpUploadPlugin::processOneService(const QDomElement& query, const QString& service, int account) {
	QString curJid = accInfo->getJid(account);
	int sizeLimit = -1;
	QDomElement feature = query.firstChildElement("feature");
	bool ok = false;
	while (!feature.isNull()) {
		if (feature.attribute("var") == "urn:xmpp:http:upload") {
#ifdef DEBUG_UPLOAD
			qDebug() << "Service" << service << "looks like http upload";
#endif
			QDomElement x = query.firstChildElement("x");
			while (!x.isNull()) {
				QDomElement field = x.firstChildElement("field");
				while (!field.isNull()) {
					if (field.attribute("var") == "max-file-size") {
						QDomElement sizeNode = field.firstChildElement("value");
						int foundSizeLimit = sizeNode.text().toInt(&ok);
						if (ok) {
#ifdef DEBUG_UPLOAD
							qDebug() << "Discovered size limit:" << foundSizeLimit;
#endif
							sizeLimit = foundSizeLimit;
							break;
						}
					}
					field = field.nextSiblingElement("field");
				}
				x = x.nextSiblingElement("x");
			}
		}
		feature = feature.nextSiblingElement("feature");
	}
	if (sizeLimit > 0) {
		serviceNames.insert(curJid, UploadService(service, sizeLimit));
	}
}

void HttpUploadPlugin::processUploadSlot(const QDomElement& xml) {
	if (xml.firstChildElement("request").attribute("xmlns") == "urn:xmpp:http:upload") {
		QDomElement error = xml.firstChildElement("error");
		if (!error.isNull()) {
			QString errorText = error.firstChildElement("text").text();
			if (!errorText.isNull()) {
				QMessageBox::critical(0, tr("Error requesting slot"), errorText);
				cancelTimeout();
				return;
			}
		}
	}
	QDomElement slot = xml.firstChildElement("slot");
	if (slot.attribute("xmlns") == "urn:xmpp:http:upload") {
		slotTimeout.stop();
		QString put = slot.firstChildElement("put").text();
		QString get = slot.firstChildElement("get").text();
#ifdef DEBUG_UPLOAD
		qDebug() << "PUT:" << put;
		qDebug() << "GET:" << get;
#endif
		if (get.isEmpty() || put.isEmpty()) {
			QMessageBox::critical(0, tr("Error requesting slot"),
					tr("Either put or get URL is missing in the server's reply."));
			cancelTimeout();
			return;
		}
		currentUpload.getUrl = get;
		QNetworkRequest req;
		req.setUrl(QUrl(put));
		if (!dataSource) {
			QMessageBox::critical(0, tr("Error uploading"),
					tr("No data to upload, this maybe a result of timeout or other error."));
			cancelTimeout();
			return;
		}
		qint64 size = dataSource->size();
		req.setHeader(QNetworkRequest::ContentLengthHeader, size);
		manager->put(req, dataSource);
	}
}

bool HttpUploadPlugin::incomingStanza(int account, const QDomElement& xml) {
	if (xml.nodeName() == "iq" && xml.attribute("type") == "result") {
		QDomElement query = xml.firstChildElement("query");
		if (!query.isNull()) {
			if (query.attribute("xmlns") == "http://jabber.org/protocol/disco#items") {
				processServices(query, account);
			}
			if (query.attribute("xmlns") == "http://jabber.org/protocol/disco#info") {
				processOneService(query, xml.attribute("from"), account);
			}
		} else {
			processUploadSlot(xml);
		}
	}
	return false;
}

void HttpUploadPlugin::uploadComplete(QNetworkReply* reply) {
	bool ok;
	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&ok);
	if (ok && (statusCode == 201 || statusCode == 200)) {
		QString id = getId(currentUpload.account);
		QString receipt(
				currentUpload.type == "chat"
						&& psiOptions->getGlobalOption("options.ui.notifications.request-receipts").toBool() ?
						"<request xmlns='urn:xmpp:receipts'/>" : "");
		QString message = QString("<message type=\"%1\" to=\"%2\" id=\"%3\">"
				"<body>%4</body>"
				"%5"
				"</message>").arg(currentUpload.type).arg(currentUpload.to).arg(id).arg(currentUpload.getUrl).arg(
				receipt);
		stanzaSender->sendStanza(currentUpload.account, message);
		if (currentUpload.type == "chat") {
			// manually add outgoing message to the regular chats, in MUC this isn't needed
			psiController->appendMsg(currentUpload.account, currentUpload.to, currentUpload.getUrl, id);
		}
		cancelTimeout();
	} else {
		QMessageBox::critical(0, tr("Error uploading"),
				tr("Upload error code %1, message: %2").arg(statusCode).arg(
						reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
	}
}

void HttpUploadPlugin::timeout() {
	if (dataSource) {
		dataSource->deleteLater();
	}
	QMessageBox::critical(0, tr("Error requesting slot"), tr("Timeout waiting for an upload slot"));
}

void HttpUploadPlugin::applyOptions() {
	psiOptions->setPluginOption(OPTION_PREVIEW_WIDTH, previewWidth = sb_previewWidth->value());
	psiOptions->setPluginOption(OPTION_RESIZE, imageResize = cb_resize->checkState() == Qt::Checked);
	psiOptions->setPluginOption(OPTION_SIZE, imageSize = sb_size->value());
	psiOptions->setPluginOption(OPTION_QUALITY, imageQuality = sb_quality->value());
}
void HttpUploadPlugin::restoreOptions() {
	sb_previewWidth->setValue(previewWidth);
	sb_size->setValue(imageSize);
	sb_quality->setValue(imageQuality);
	cb_resize->setCheckState(imageResize ? Qt::Checked : Qt::Unchecked);
}

void HttpUploadPlugin::resizeStateChanged(int state) {
	bool enabled = state == Qt::Checked;
	sb_size->setEnabled(enabled);
	sb_quality->setEnabled(enabled);
}

void HttpUploadPlugin::setApplicationInfoAccessingHost(ApplicationInfoAccessingHost* host) {
	appInfoHost = host;
}

void HttpUploadPlugin::updateProxy() {
	Proxy proxy = appInfoHost->getProxyFor(name());
#ifdef DEBUG_UPLOAD
	qDebug() << "Proxy:" << "T:" << proxy.type << "H:" << proxy.host << "Pt:" << proxy.port << "U:" << proxy.user
	<< "Ps:" << proxy.pass;
#endif
	if (proxy.type.isEmpty()) {
		manager->setProxy(QNetworkProxy());
		return;
	}
	QNetworkProxy netProxy(proxy.type == "socks" ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy, proxy.host,
			proxy.port, proxy.user, proxy.pass);
	manager->setProxy(netProxy);
}

#include "httpuploadplugin.moc"
