#ifndef INVITEDLG_H
#define INVITEDLG_H

#include <QtGui>

namespace XMPP
{
	class Jid;
}

class PsiAccount;

class InviteDlg : public QDialog
{
	Q_OBJECT
public:
	// if groupchat is true, jid is a user's own muc jid (with resource)
	// if groupchat is false, jid is the jid of the other contact
	InviteDlg(bool groupchat, const XMPP::Jid &jid, PsiAccount *pa, QWidget *parent = 0);
	~InviteDlg();

private:
	class Private;
	Private *d;
};

#endif
