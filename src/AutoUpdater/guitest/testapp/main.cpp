/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#include <QApplication>
#include <QDir>
#include <QWidget>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QFile>

#include "AutoUpdater/AutoUpdater.h"
#include "AutoUpdater/SparkleAutoUpdater.h"
#include "CocoaUtilities/CocoaInitializer.h"

class SimpleHTTPServer : public QObject
{
		Q_OBJECT

	public:
		SimpleHTTPServer(int port = 0) {
      tcpServer_ = new QTcpServer(this);
			if (!tcpServer_->listen(QHostAddress::Any, port)) {
				Q_ASSERT(false);
			}
		  connect(tcpServer_, SIGNAL(newConnection()), this, SLOT(incomingConnection()));
		};

		~SimpleHTTPServer() {
			delete tcpServer_;
		}

		unsigned int getPort() {
			return tcpServer_->serverPort();
		}

		void setPage(const QString& url, const QByteArray& content) {
			pages_[url] = content;
		}

	public slots:
		void incomingConnection() {
			QTcpSocket* socket = tcpServer_->nextPendingConnection();
			connect(socket, SIGNAL(readyRead()), SLOT(readBytes()));
		}

		void readBytes() {
			QTcpSocket* socket = static_cast<QTcpSocket*>(sender());
			QByteArray data = socket->readAll();

			// FIXME: Only do this for the initial request.
			// Wait for the request to finish
			QList<QByteArray> tokens = data.split(' ');
			if (tokens[0] == QString("GET")) {
				if (!pages_.contains(tokens[1])) {
					socket->write("HTTP/1.1 404 Not Found\n\n");
					Q_ASSERT(false);
				}

				QByteArray responseData = pages_[tokens[1]];

				QString response;
				response += "HTTP/1.1 200 OK\n";
				if (tokens[1].endsWith(".html")) {
					response += "Content-Type: text/html\n";
				}
				if (tokens[1].endsWith(".xml")) {
					response += "Content-Type: application/xml\n";
				}
				else {
					response += "Content-Type: text/plain\n";
				}
				response += "Content-Length: " + QString::number(responseData.size()) + "\n";
				response += "\n";
				socket->write(response.toUtf8());
				socket->write(responseData);
			}
		}
	
	private:
		QTcpServer *tcpServer_;
		QMap<QString,QByteArray> pages_;
};

class UpdaterWidget : public QWidget
{
		Q_OBJECT

	public:
		UpdaterWidget() {
			QFile(QDir::homePath() + "/Library/Preferences/org.psi-im.autoupdater-test.plist").remove();
			QVBoxLayout* layout = new QVBoxLayout(this);

			QPushButton* newButton = new QPushButton("Check with new release", this);
			connect(newButton, SIGNAL(clicked()), this, SLOT(doCheckWithNew()));
			layout->addWidget(newButton);

			QPushButton* noNewButton = new QPushButton("Check without new release", this);
			connect(noNewButton, SIGNAL(clicked()), this, SLOT(doCheckWithoutNew()));
			layout->addWidget(noNewButton);

			server_ = new SimpleHTTPServer();

			QString baseURL = "http://localhost:" + QString::number(server_->getPort());
			appCastURL_ = baseURL + "/appcast.xml";
			releaseNotesURL_ = baseURL + "/releasenotes.html";
			releaseURL_ = baseURL + "/testapp-2.0.zip";

			QString releaseNotes = "<html><body><h2>My release notes</h2><ul><li>note 1</li><li>note 2</li></body></html>";

			server_->setPage("/releasenotes.html", releaseNotes.toUtf8());
			QFile app(":testapp-2.0.zip");
			app.open(QIODevice::ReadOnly);
			server_->setPage("/testapp-2.0.zip", app.readAll());
			app.close();

			updater_ = new SparkleAutoUpdater(appCastURL_);
		}

		~UpdaterWidget() {
			delete updater_;
			delete server_;
		}

		QByteArray createAppCast(bool withNewVersion = false)
		{
			QString appcast;
			appcast += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
			appcast += "<rss version=\"2.0\" xmlns:sparkle=\"http://www.andymatuschak.org/xml-namespaces/sparkle\"  xmlns:dc=\"http://purl.org/dc/elements/1.1/\">";
			appcast += " <channel>";
			appcast += "  <title>Your Great App's Changelog</title>";
			appcast += "  <link>" + appCastURL_ + "</link>";
			appcast += "  <description>Most recent changes with links to updates.</description>";
			if (withNewVersion) {
				appcast += "  <item>";
				appcast += "   <title>Version 2.0 (2 bugs fixed; 3 new features)</title>";
				appcast += "   <sparkle:releaseNotesLink> " + releaseNotesURL_ + "</sparkle:releaseNotesLink>";
				appcast += "   <pubDate>Wed, 09 Jan 2006 19:20:11 +0000</pubDate>";
				appcast += "   <enclosure url=\"" + releaseURL_ + "\" sparkle:version=\"2.0\" length=\"1623481\" type=\"application/octet-stream\"/>";
				appcast += "  </item>";
			}

			appcast += "  <item>";
			appcast += "   <title>Version 1.0 (2 bugs fixed; 3 new features)</title>";
			appcast += "   <sparkle:releaseNotesLink> " + releaseNotesURL_ + "</sparkle:releaseNotesLink>";
			appcast += "   <pubDate>Wed, 08 Jan 2006 19:20:11 +0000</pubDate>";
			appcast += "   <enclosure url=\"" + releaseURL_ + "\" sparkle:version=\"1.0\" length=\"1623481\" type=\"application/octet-stream\"/>";
			appcast += "  </item>";

			appcast += " </channel>";
			appcast += "</rss>";
			return appcast.toUtf8();
		} 


	public slots:
		void doCheckWithNew() {
			server_->setPage("/appcast.xml", createAppCast(true));
			updater_->checkForUpdates();
		}

		void doCheckWithoutNew() {
			server_->setPage("/appcast.xml", createAppCast(false));
			updater_->checkForUpdates();
		}

	private:
		AutoUpdater* updater_;
		SimpleHTTPServer* server_;
		QString appCastURL_, releaseNotesURL_, releaseURL_;
};

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	CocoaInitializer initializer;
	UpdaterWidget w;
	w.show();
	app.exec();
	return 0;
}

#include "main.moc"
