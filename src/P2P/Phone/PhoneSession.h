#ifndef PHONESESSION_H
#define PHONESESSION_H

#include <QObject>

#include "xmpp/jid/jid.h"

class PhoneSession : public QObject
{
		Q_OBJECT

	public:
		enum State {
			State_Initial,
			State_Calling,
			State_Accepting,
			State_Rejecting,
			State_Terminating,
			State_Accepted,
			State_Rejected,
			State_InProgress,
			State_Terminated,
			State_Incoming
		};

		PhoneSession();
		virtual ~PhoneSession();

		virtual const XMPP::Jid& getPeer() const = 0;

		virtual void start() = 0;
		virtual void accept() = 0;
		virtual void reject() = 0;
		virtual void terminate() = 0;
		
		State getState() const { return state_; }
	
	protected:
		void setState(State);

	signals:
		void stateChanged(PhoneSession::State);

	private:
		State state_;
};

#endif
