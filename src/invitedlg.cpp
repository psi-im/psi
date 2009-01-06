#include "invitedlg.h"

#include "ui_invite.h"
#include "xmpp.h"
#include "psiaccount_b.h"
#include "userlist.h"
#include "jidutil.h"

class InviteDlg::Private : public QObject
{
	Q_OBJECT

public:
	enum InviteType { GroupChat, NormalChat };

	InviteDlg *q;
	Ui::Invite ui;
	PsiAccount *pa;
	InviteType inviteType;
	XMPP::Jid inviteJid;

	Private(InviteDlg *_q, PsiAccount *_pa) :
		QObject(_q),
		q(_q),
		pa(_pa)
	{
		ui.setupUi(q);
		ui.lbx_users->setSortingEnabled(true);

		connect(ui.buttonBox, SIGNAL(accepted()), SLOT(pb_accept()));

		// populate list with online contacts
		foreach(UserListItem *u, *(pa->userList()))
		{
			if(u->isAvailable())
			{
				QString nick = JIDUtil::nickOrJid(u->name(), u->jid().full());

				QListWidgetItem *i = new QListWidgetItem(nick, ui.lbx_users);
				i->setData(Qt::UserRole, u->jid().full());
				i->setCheckState(Qt::Unchecked);
			}
		}
	}

private slots:
	void pb_accept()
	{
		QList<XMPP::Jid> list;
		for(int n = 0; n < ui.lbx_users->count(); ++n)
		{
			QListWidgetItem *i = ui.lbx_users->item(n);
			if(i->checkState() == Qt::Checked)
				list += i->data(Qt::UserRole).toString();
		}

		qRegisterMetaType< XMPP::Jid >("XMPP::Jid");
		qRegisterMetaType< QList<XMPP::Jid> >("QList<XMPP::Jid>");

		QMetaObject::invokeMethod(pa, "doMultiInvite",
			Qt::QueuedConnection,
			Q_ARG(QList<XMPP::Jid>, list),
			Q_ARG(bool, inviteType == GroupChat ? true : false),
			Q_ARG(XMPP::Jid, inviteJid));

		q->accept();
	}
};

InviteDlg::InviteDlg(bool groupchat, const XMPP::Jid &jid, PsiAccount *pa, QWidget *parent) :
	QDialog(parent)
{
	d = new Private(this, pa);

	if(groupchat)
		d->inviteType = Private::GroupChat;
	else
		d->inviteType = Private::NormalChat;
	d->inviteJid = jid;
}

InviteDlg::~InviteDlg()
{
	delete d;
}

#include "invitedlg.moc"
