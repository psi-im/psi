/*
 * googleftmanager.cpp
 * Copyright (C) 2006  Remko Troncon
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
// libjingle includes
#define POSIX
#include "talk/base/sigslot.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlprinter.h"
#include "talk/base/network.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/base/helpers.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/client/sessionsendtask.h"
#include "talk/p2p/client/httpportallocator.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/thread.h"
#include "talk/base/socketaddress.h"
#include "talk/session/fileshare/fileshare.h"

#include <QString>
#include <QDomElement>
#include <QDir>

#include "xmpp_xmlcommon.h"
#include "googleftmanager.h"

// Should change in the future
#define JINGLE_NS "http://www.google.com/session"
#define JINGLEINFO_NS "google:jingleinfo"

using namespace XMPP;

// ----------------------------------------------------------------------------

/**
 * \class GoogleJingleInfoTask
 * A class for retrieving information from the server about Google's 
 * Jingle support.
 */
class GoogleJingleInfoTask : public Task
{
public:
	GoogleJingleInfoTask(Task* parent) : Task(parent) {
	}

	void onGo() {
		QDomElement iq = createIQ(doc(), "get", "", id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns", JINGLEINFO_NS);
		iq.appendChild(query);
		send(iq);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			// TODO:Parse info
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}
};


// ----------------------------------------------------------------------------

/**
 * \class JingleIQResponder
 * \brief A task that ensures that we don't send an unsupported error
 */
class JingleIQResponder : public XMPP::Task
{
public:
	JingleIQResponder(XMPP::Task * parent) : Task(parent) {}
	~JingleIQResponder() {}

	bool take(const QDomElement& e) {
		if(e.tagName() != "iq")
			return false;
		QDomElement first = e.firstChild().toElement();
		if (!first.isNull() && first.attribute("xmlns") == JINGLE_NS) {
			return true;
		}
		return false;
	}
};

// ----------------------------------------------------------------------------

/**
 * \brief A class for handling signals from libjingle.
 */

class GoogleSessionListener : public sigslot::has_slots<> 
{
public:
	GoogleSessionListener(GoogleFTManager* manager);
	void fileShareSessionCreated(cricket::FileShareSession *);
	void sendStanza(const buzz::XmlElement *stanza);
	void signalingReady();

private:
	GoogleFTManager* manager_;
};


GoogleSessionListener::GoogleSessionListener(GoogleFTManager* manager) : manager_(manager)
{
}

void GoogleSessionListener::sendStanza(const buzz::XmlElement *stanza) 
{
	QString st(stanza->Str().c_str());
	st.replace("<sta:","<");
	st.replace("</sta:","</");
	st.replace("<cli:","<");
	st.replace("</cli:","</");
	st.replace(":cli=","=");
	st.replace("xmlns:sta","xmlns");
	manager_->sendStanza(st);
}

void GoogleSessionListener::fileShareSessionCreated(cricket::FileShareSession* session)
{
	new GoogleFileTransfer(session, manager_);
}

void GoogleSessionListener::signalingReady()
{
	manager_->session_manager_->OnSignalingReady();
}

// ----------------------------------------------------------------------------

/**
 * \brief A class for handling signals from libjingle.
 */
class GoogleFileTransferListener : public sigslot::has_slots<> 
{
public:
	GoogleFileTransferListener(GoogleFileTransfer*);
	void stateChanged(cricket::FileShareState);
	void progressChanged(cricket::FileShareSession*);
	void resampleImage(std::string path, int width, int height, talk_base::HttpTransaction* trans);

private:
	GoogleFileTransfer* session_;
};

GoogleFileTransferListener::GoogleFileTransferListener(GoogleFileTransfer* s) : session_(s)
{
}

void GoogleFileTransferListener::stateChanged(cricket::FileShareState state)
{
	switch(state) {
		case cricket::FS_OFFER:
			emit session_->manager_->incomingFileTransfer(session_);
			break;
			
		case cricket::FS_TRANSFER:
			qDebug("Transfer started");
			break;
			
		case cricket::FS_COMPLETE:
			qDebug("Transfer complete");
			break;

		case cricket::FS_LOCAL_CANCEL:
		case cricket::FS_REMOTE_CANCEL:
			qDebug("FS_CANCEL");
			break;

		case cricket::FS_FAILURE:
			qDebug("FS_FAILURE");
			break;
	}
}

void GoogleFileTransferListener::progressChanged(cricket::FileShareSession* sess)
{
	size_t progress;
	std::string itemname;
	if (sess->GetProgress(progress) && sess->GetCurrentItemName(&itemname)) {
		emit session_->progressChanged(progress,QString(itemname.c_str()));
	}
}

void GoogleFileTransferListener::resampleImage(std::string, int, int, talk_base::HttpTransaction* trans)
{
	// From PCP
	session_->session_->ResampleComplete(NULL, trans, false);
}

// ----------------------------------------------------------------------------

GoogleFileTransfer::GoogleFileTransfer(cricket::FileShareSession* s, GoogleFTManager* manager) : session_(s), manager_(manager)
{
	listener_ = new GoogleFileTransferListener(this);
    session_->SignalState.connect(listener_, &GoogleFileTransferListener::stateChanged);
    session_->SignalNextFile.connect(listener_, &GoogleFileTransferListener::progressChanged);
    session_->SignalUpdateProgress.connect(listener_, &GoogleFileTransferListener::progressChanged);
    session_->SignalResampleImage.connect(listener_, &GoogleFileTransferListener::resampleImage);

	// Temporary
#ifdef Q_WS_MAC
	QDir home(QDir::homeDirPath() + "/Desktop");
#else
	QDir home = QDir::home();
#endif
	QDir dir(home.path() + "/googletalk_files");
	if(!dir.exists()) 
		home.mkdir("googletalk_files");
    session_->SetLocalFolder(dir.path().toStdString());
}

XMPP::Jid GoogleFileTransfer::peer() const 
{ 
	return Jid(session_->jid().BareJid().Str().c_str());
}

QString GoogleFileTransfer::fileName() const 
{
	return description();
}

QString GoogleFileTransfer::description() const 
{ 
	QString description;
	if (session_->manifest()->size() == 1)
		description = QString("'%1'").arg(session_->manifest()->item(0).name.c_str());
	else if (session_->manifest()->GetFileCount() && session_->manifest()->GetFolderCount()) 
		description = QString("%1 files and %2 directories").arg(session_->manifest()->GetFileCount()).arg(session_->manifest()->GetFolderCount());
	else if (session_->manifest()->GetFileCount())
		description = QString("%1 files").arg(session_->manifest()->GetFileCount());
	else if (session_->manifest()->GetFolderCount())
		description = QString("%1 directories").arg(session_->manifest()->GetFolderCount());
	else 
		description = "(Unknown)";
	return description;
}

qlonglong GoogleFileTransfer::fileSize() const 
{ 
	size_t filesize;
	if (!session_->GetTotalSize(filesize)) 
		filesize = -1;
	return filesize;
}

void GoogleFileTransfer::accept(qlonglong, qlonglong)
{
	session_->Accept();
}

void GoogleFileTransfer::reject()
{
	session_->Decline();
}

void GoogleFileTransfer::cancel()
{
	session_->Cancel();
}

// ----------------------------------------------------------------------------

GoogleFTManager::GoogleFTManager(Client* client) : client_(client) 
{
	initialized_ = false;
	connect(client_, SIGNAL(rosterRequestFinished(bool, int, const QString &)), SLOT(initialize()));
	connect(client_, SIGNAL(disconnected()), SLOT(deinitialize()));
}

void GoogleFTManager::initialize()
{
	if (initialized_)
		return;

	QString jid = ((ClientStream&) client_->stream()).jid().full();
	if (jid.isEmpty()) {
		qWarning("googleftmanager.cpp: Empty JID");
		return;
	}
	buzz::Jid j(jid.ascii()); // FIXME: Ascii is evil

	// Static stuff
	if (socket_server_ == NULL) {
		//talk_base::InitializeSSL();
		cricket::InitRandom(j.Str().c_str(),j.Str().size());
		socket_server_ = new talk_base::PhysicalSocketServer();
		thread_ = new talk_base::Thread(socket_server_);
		talk_base::ThreadManager::SetCurrent(thread_);
		thread_->Start();
		network_manager_ = new talk_base::NetworkManager();
		port_allocator_.reset(new cricket::HttpPortAllocator(network_manager_, "psi"));
	}

	listener_ = new GoogleSessionListener(this); 
	session_manager_.reset(new cricket::SessionManager(port_allocator_.get(), NULL));
	session_manager_->SignalOutgoingMessage.connect(listener_, &GoogleSessionListener::sendStanza);
	//session_manager_->SignalRequestSignaling.connect(session_manager_, &cricket::SessionManager::OnSignalingReady);
	session_manager_->SignalRequestSignaling.connect(listener_, &GoogleSessionListener::signalingReady);
	file_share_session_client_.reset(new cricket::FileShareSessionClient(session_manager_.get(), j, "psi"));
	file_share_session_client_->SignalFileShareSessionCreate.connect(listener_, &GoogleSessionListener::fileShareSessionCreated);
	session_manager_->AddClient(NS_GOOGLE_SHARE, file_share_session_client_.get());

	// Listen to incoming packets
	connect(client_,SIGNAL(xmlIncoming(const QString&)),SLOT(receiveStanza(const QString&)));
	
	// IQ Responder
	new JingleIQResponder(client_->rootTask());

	initialized_ = true;
}


void GoogleFTManager::deinitialize()
{
	if (!initialized_)
		return;

	// Stop listening to incoming packets
	disconnect(client_,SIGNAL(xmlIncoming(const QString&)),this,SLOT(receiveStanza(const QString&)));

	delete listener_;
	initialized_ = false;
}


GoogleFTManager::~GoogleFTManager()
{
}

void GoogleFTManager::sendStanza(const QString& stanza)
{
	client_->send(stanza);
}

void GoogleFTManager::receiveStanza(const QString& sstanza)
{
	// Add a namespace to the element (for libjingle to process correctly)
	QDomDocument doc;
	doc.setContent(sstanza);
	/*doc.documentElement().setAttribute("xmlns","jabber:client");
	QString stanza = doc.toString();
	buzz::XmlElement *e = buzz::XmlElement::ForStr(stanza.ascii());
	if (!session_manager_.get()->IsSessionMessage(e))
		return;*/
	
	QDomNode n = doc.documentElement().firstChild();
	bool ok = false;
	while (!n.isNull() && !ok) {
		QDomElement e = n.toElement();
		if (!e.isNull() && e.attribute("xmlns") == JINGLE_NS) {
			ok = true;
		}
		n = n.nextSibling();
	}
	if (!ok)
		return;
	buzz::XmlElement *e = buzz::XmlElement::ForStr(sstanza.ascii());
	
	session_manager_->OnIncomingMessage(e);
}

talk_base::PhysicalSocketServer* GoogleFTManager::socket_server_ = NULL;
talk_base::Thread *GoogleFTManager::thread_ = NULL;
talk_base::NetworkManager* GoogleFTManager::network_manager_ = NULL;
talk_base::scoped_ptr<cricket::HttpPortAllocator> GoogleFTManager::port_allocator_;
