#include <QApplication>
#include <QTimer>

#include "xmpp/jid/jid.h"
#include "P2P/Phone/PhoneSession.h"
#include "Phone/PhoneCallDialog.h"

class MyPhoneSession : public PhoneSession
{
		Q_OBJECT

	public:
		MyPhoneSession(const XMPP::Jid& peer) : peer_(peer) {}

		void setOtherSession(MyPhoneSession* session) {
			other_ = session;
		}

		virtual const XMPP::Jid& getPeer() const { return peer_; }

		virtual void start() {
			setState(State_Calling);
			other_->setState(State_Incoming);
		}

		virtual void accept() {
			setState(State_Accepting);
			other_->setState(State_Accepted);
			QTimer::singleShot(2000, this, SLOT(handleActionAcknowledged()));
			QTimer::singleShot(1000, other_, SLOT(handleActionAcknowledged()));
		}

		virtual void reject() {
			setState(State_Rejecting);
			other_->setState(State_Rejected);
			QTimer::singleShot(2000, this, SLOT(handleActionAcknowledged()));
		}

		virtual void terminate() {
			setState(State_Terminating);
			other_->setState(State_Terminated);
			QTimer::singleShot(2000, this, SLOT(handleActionAcknowledged()));
		}

	private slots:
		void handleActionAcknowledged() {
			if (getState() == State_Terminating) {
				setState(State_Terminated);
			}
			else if (getState() == State_Rejecting) {
				setState(State_Rejected);
			}
			else if (getState() == State_Accepting || getState() == State_Accepted) {
				setState(State_InProgress);
			}
			else {
				Q_ASSERT(false);
			}
		}

	private:
		XMPP::Jid peer_;
		MyPhoneSession* other_;
};

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	MyPhoneSession session1("juliet@capulet.com/balcony"), session2("romeo@montague.net/orchard");
	session1.setOtherSession(&session2);
	session2.setOtherSession(&session1);

	PhoneCallDialog dlg1(&session1);
	dlg1.show();

	PhoneCallDialog dlg2(&session2);
	dlg2.show();

	dlg1.startCall();

	return app.exec();
}

#include "guitest.moc"
