#include <QtCore>
#include <QStringList>
#include <QDialog>

#include "pgputil.h"
#include "passphrasedlg.h"

PGPUtil::PGPUtil() : qcaEventHandler_(NULL), passphraseDlg_(NULL)
{
}

PGPUtil& PGPUtil::instance()
{
	if (!instance_) {
		instance_ = new PGPUtil();
	}
	return *instance_;
}

void PGPUtil::setEventHandler(QCA::EventHandler* e) 
{
	qcaEventHandler_ = e;
}

void PGPUtil::handleEvent(int id, const QCA::Event& event)
{
	if (event.type() == QCA::Event::Password) {
		QCA::KeyStoreEntry entry = event.keyStoreEntry();
		if(!entry.isNull() && passphrases_.contains(entry.id())) {
			qcaEventHandler_->submitPassword(id,QCA::SecureArray(passphrases_[entry.id()].utf8()));
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
				qcaEventHandler_->submitPassword(eventItem.id,QCA::SecureArray(passphrases_[entry.id()].utf8()));
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
	return (QCA::isSupported("openpgp") && keystores.count() > 0);
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
	QStringList lines = QStringList::split('\n', str, true);
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
	s += "\n";
	s += str + '\n';
	s += QString("-----END PGP %1-----\n").arg(stype);
	return s;
}


QCA::KeyStoreEntry PGPUtil::getSecretKeyStoreEntry(const QString& keyID)
{
	foreach(QCA::KeyStore *ks, PGPUtil::keystores) {
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
	foreach(QCA::KeyStore *ks, PGPUtil::keystores) {
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


PGPUtil* PGPUtil::instance_ = NULL;
