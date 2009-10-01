#include "pgputil.h"

#include <QtCore>
#include <QStringList>
#include <QDialog>
#include <QMessageBox>

#include "passphrasedlg.h"
#include "showtextdlg.h"

PGPUtil* PGPUtil::instance_ = 0;

PGPUtil::PGPUtil() : qcaEventHandler_(NULL), passphraseDlg_(NULL), cache_no_pgp_(false)
{
	qcaEventHandler_ = new QCA::EventHandler(this);
	connect(qcaEventHandler_,SIGNAL(eventReady(int,const QCA::Event&)),SLOT(handleEvent(int,const QCA::Event&)));
	qcaEventHandler_->start();
	qcaKeyStoreManager_.waitForBusyFinished(); // FIXME get rid of this
	connect(&qcaKeyStoreManager_, SIGNAL(keyStoreAvailable(const QString&)), SLOT(keyStoreAvailable(const QString&)));
	foreach(QString k, qcaKeyStoreManager_.keyStores()) {
		QCA::KeyStore* ks = new QCA::KeyStore(k, &qcaKeyStoreManager_);
		connect(ks, SIGNAL(updated()), SIGNAL(pgpKeysUpdated()));
		keystores_ += ks;
	}

	connect(QCoreApplication::instance(),SIGNAL(aboutToQuit()),SLOT(deleteLater()));
}

PGPUtil::~PGPUtil()
{
	foreach(QCA::KeyStore* ks,keystores_)  {
		delete ks;
	}
	keystores_.clear();
}


PGPUtil& PGPUtil::instance()
{
	if (!instance_) {
		instance_ = new PGPUtil();
	}
	return *instance_;
}

void PGPUtil::handleEvent(int id, const QCA::Event& event)
{
	if (event.type() == QCA::Event::Password) {
		QCA::KeyStoreEntry entry = event.keyStoreEntry();
		if(!entry.isNull() && passphrases_.contains(entry.id())) {
			qcaEventHandler_->submitPassword(id, QCA::SecureArray(passphrases_[entry.id()].toUtf8()));
		}
		else if (passphraseDlg_) {
			EventItem i;
			i.id = id;
			i.event = event;
			pendingEvents_.push_back(i);
		}
		else {
			promptPassphrase(id,event);
		}
	}
}

void PGPUtil::promptPassphrase(int id, const QCA::Event& event)
{
	QString name;
	currentEventId_ = id;

	QCA::KeyStoreEntry entry = event.keyStoreEntry();
	if(!entry.isNull()) {
		name = entry.name();
		currentEntryId_ = entry.id(); 
	}
	else {
		name = event.keyStoreInfo().name();
		currentEntryId_ = QString();
	}
	
	if (!passphraseDlg_) {
		passphraseDlg_ = new PassphraseDlg();
		connect(passphraseDlg_,SIGNAL(finished(int)),SLOT(passphraseDone(int)));
	}
	passphraseDlg_->promptPassphrase(name);
	passphraseDlg_->show();
}

void PGPUtil::passphraseDone(int result)
{
	// Process the result
	if (result == QDialog::Accepted) {
		QString passphrase = passphraseDlg_->getPassphrase();
		if (!currentEntryId_.isEmpty()) {
			passphrases_[currentEntryId_] = passphrase;
		}
		qcaEventHandler_->submitPassword(currentEventId_,passphrase.toUtf8());
	}
	else if (result == QDialog::Rejected) {
		qcaEventHandler_->reject(currentEventId_);
	}
	else {
		qWarning() << "PGPUtil: Unexpected passphrase dialog result";
	}
	
	// Process the queue
	if (!pendingEvents_.isEmpty()) {
		EventItem eventItem;
		bool handlePendingEvent = false;
		while (!pendingEvents_.isEmpty() && !handlePendingEvent) {
			eventItem = pendingEvents_.takeFirst();
			QCA::KeyStoreEntry entry = eventItem.event.keyStoreEntry();
			if(!entry.isNull() && passphrases_.contains(entry.id())) {
				qcaEventHandler_->submitPassword(eventItem.id, QCA::SecureArray(passphrases_[entry.id()].toUtf8()));
			}
			else {
				handlePendingEvent = true;
			}
		}
		if (handlePendingEvent) {
			promptPassphrase(eventItem.id,eventItem.event);
			return;
		}
	}
	passphraseDlg_->deleteLater();
	passphraseDlg_ = NULL;
}

bool PGPUtil::pgpAvailable()
{
	bool have_openpgp = false;
	if(!cache_no_pgp_) {
		have_openpgp = QCA::isSupported("openpgp");
		if(!have_openpgp)
			cache_no_pgp_ = true;
	}
	return (have_openpgp && keystores_.count() > 0);
}

void PGPUtil::clearPGPAvailableCache()
{
	cache_no_pgp_ = false;
}

QString PGPUtil::stripHeaderFooter(const QString &str)
{
	QString s;
	if (str.isEmpty()) {
		qWarning("pgputil.cpp: Empty PGP message");
		return "";
	}
	if(str.at(0) != '-')
		return str;
	QStringList lines = str.split('\n');
	QStringList::ConstIterator it = lines.begin();
	// skip the first line
	++it;
	if(it == lines.end())
		return str;

	// skip the header
	for(; it != lines.end(); ++it) {
		if((*it).isEmpty())
			break;
	}
	if(it == lines.end())
		return str;
	++it;
	if(it == lines.end())
		return str;

	bool first = true;
	for(; it != lines.end(); ++it) {
		if((*it).at(0) == '-')
			break;
		if(!first)
			s += '\n';
		s += (*it);
		first = false;
	}

	return s;
}


QString PGPUtil::addHeaderFooter(const QString &str, int type)
{
	QString stype;
	if(type == 0)
		stype = "MESSAGE";
	else
		stype = "SIGNATURE";

	QString s;
	s += QString("-----BEGIN PGP %1-----\n").arg(stype);
	s += "Version: PGP\n";
	s += '\n';
	s += str + '\n';
	s += QString("-----END PGP %1-----\n").arg(stype);
	return s;
}


QCA::KeyStoreEntry PGPUtil::getSecretKeyStoreEntry(const QString& keyID)
{
	foreach(QCA::KeyStore *ks, keystores_) {
		if (ks->type() == QCA::KeyStore::PGPKeyring && ks->holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks->entryList()) {
				if (ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey
				    && ke.pgpSecretKey().keyId() == keyID) {
					return ke;
				}
			}
		}
	}
	return QCA::KeyStoreEntry();
}

QCA::KeyStoreEntry PGPUtil::getPublicKeyStoreEntry(const QString& keyID)
{
	foreach(QCA::KeyStore *ks, keystores_) {
		if (ks->type() == QCA::KeyStore::PGPKeyring && ks->holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks->entryList()) {
				if ((ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey
				     || ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey)
				    && ke.pgpPublicKey().keyId() == keyID) {
					return ke;
				}
			}
		}
	}
	return QCA::KeyStoreEntry();
}

QString PGPUtil::messageErrorString(enum QCA::SecureMessage::Error e)
{
	QString msg;
	switch(e) {
		case QCA::SecureMessage::ErrorPassphrase:
			msg = QObject::tr("Invalid passphrase");
			break;
		case QCA::SecureMessage::ErrorFormat:
			msg = QObject::tr("Invalid input format");
			break;
		case QCA::SecureMessage::ErrorSignerExpired:
			msg = QObject::tr("Signing key expired");
			break;
		case QCA::SecureMessage::ErrorSignerInvalid:
			msg = QObject::tr("Invalid key");
			break;
		case QCA::SecureMessage::ErrorEncryptExpired:
			msg = QObject::tr("Encrypting key expired");
			break;
		case QCA::SecureMessage::ErrorEncryptUntrusted:
			msg = QObject::tr("Encrypting key is untrusted");
			break;
		case QCA::SecureMessage::ErrorEncryptInvalid:
			msg = QObject::tr("Encrypting key is invalid");
			break;
		case QCA::SecureMessage::ErrorNeedCard:
			msg = QObject::tr("PGP card is missing");
			break;
		default:
			msg = QObject::tr("Unknown error");
	}
	return msg;
}

bool PGPUtil::equals(QCA::PGPKey k1, QCA::PGPKey k2)
{
	if (k1.isNull()) {
		return k2.isNull();
	}
	else if (k2.isNull()) {
		return false;
	}
	else {
		return k1.keyId() == k2.keyId();
	}
}

void PGPUtil::removePassphrase(const QString& id)
{
	passphrases_.remove(id);
}

void PGPUtil::keyStoreAvailable(const QString& k)
{
	QCA::KeyStore* ks = new QCA::KeyStore(k, &qcaKeyStoreManager_);
	connect(ks, SIGNAL(updated()), SIGNAL(pgpKeysUpdated()));
	keystores_ += ks;
}

void PGPUtil::showDiagnosticText(const QString& event, const QString& diagnostic)
{
	while (1) {
		QMessageBox msgbox(QMessageBox::Critical,
		                   tr("Error"),
		                   event,
		                   QMessageBox::Ok, 0);
		QPushButton *diag = msgbox.addButton(tr("Diagnostics"), QMessageBox::HelpRole);
		msgbox.exec();
		if (msgbox.clickedButton() == diag) {
			ShowTextDlg* w = new ShowTextDlg(diagnostic, true, false, 0);
			w->setWindowTitle(tr("OpenPGP Diagnostic Text"));
			w->resize(560, 240);
			w->exec();

			continue;
		}
		else {
			break;
		}
	}
}

void PGPUtil::showDiagnosticText(QCA::SecureMessage::Error error, const QString& diagnostic)
{
	showDiagnosticText(tr("There was an error trying to send the message encrypted.\nReason: %1.")
	                   .arg(PGPUtil::instance().messageErrorString(error)),
	                   diagnostic);
}
