#include "P2P/Phone/PhoneSession.h"
 
PhoneSession::PhoneSession() : state_(State_Initial)
{
}

PhoneSession::~PhoneSession()
{
}

void PhoneSession::setState(State state)
{
	state_ = state;
	emit stateChanged(state_);
}
