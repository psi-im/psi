#include "transportsetupdlg.h"

#include "ui_transportsetup.h"
#include "psiaccount_b.h"
#include "xmpp_tasks.h"
#include "iconset.h"

using namespace XMPP;

#define FAKE_PASS "********"

class TransportSetupDlg::Private : public QObject
{
	Q_OBJECT

public:
	TransportSetupDlg *q;
	Ui::TransportSetup ui;
	PsiAccount *pa;

	class Transport
	{
	public:
		Jid jid;
		JT_Register *reg;
		bool done;
		bool avail;

		QString cur_user;
		QString cur_nick;
		QString cur_pass;

		QGroupBox *gb;
		QLineEdit *le_user;
		QLineEdit *le_nick;
		QLineEdit *le_pass;

		Transport() :
			reg(0),
			gb(0),
			le_user(0),
			le_nick(0),
			le_pass(0)
		{
		}
	};

	QList<Transport> list;

	bool do_set;

	Private(TransportSetupDlg *_q, PsiAccount *_pa) :
		QObject(_q),
		q(_q),
		pa(_pa)
	{
		ui.setupUi(q);

		{
			//int max = 0;
			//QLabel *lb_max = 0;
			QLabel *lb_array[9];
			lb_array[0] = ui.lb_user_aim;
			lb_array[1] = ui.lb_user_msn;
			lb_array[2] = ui.lb_user_yahoo;
			lb_array[3] = ui.lb_user_icq;
			lb_array[4] = ui.lb_pass_aim;
			lb_array[5] = ui.lb_pass_msn;
			lb_array[6] = ui.lb_pass_yahoo;
			lb_array[7] = ui.lb_pass_icq;
			lb_array[8] = ui.lb_nick_msn;
			/*for(int n = 0; n < 4; ++n)
			{
				int wid = lb_array[n]->minimumWidth();
				if(!lb_max || wid > max)
				{
					lb_max = lb_array[n];
					max = wid;
				}
			}*/
			for(int n = 0; n < 9; ++n)
			{
				//if(lb_array[n] != lb_max)
					lb_array[n]->setFixedWidth(80);
			}
		}

		ui.lb_img_aim->setPixmap(IconsetFactory::icon("psi/aimlogo").pixmap());
		ui.lb_img_msn->setPixmap(IconsetFactory::icon("psi/msnlogo").pixmap());
		ui.lb_img_yahoo->setPixmap(IconsetFactory::icon("psi/yahoologo").pixmap());
		ui.lb_img_icq->setPixmap(IconsetFactory::icon("psi/icqlogo").pixmap());

		ui.lb_euser_aim->setEnabled(false);
		ui.lb_euser_msn->setEnabled(false);
		ui.lb_euser_yahoo->setEnabled(false);

		Transport t1;
		t1.jid = "aim.transport";
		t1.gb = ui.gb_aim;
		t1.le_user = ui.le_user_aim;
		t1.le_pass = ui.le_pass_aim;
		list += t1;
		Transport t2;
		t2.jid = "msn.transport";
		t2.gb = ui.gb_msn;
		t2.le_user = ui.le_user_msn;
		t2.le_nick = ui.le_nick_msn;
		t2.le_pass = ui.le_pass_msn;
		list += t2;
		Transport t3;
		t3.jid = "yahoo.transport";
		t3.gb = ui.gb_yahoo;
		t3.le_user = ui.le_user_yahoo;
		t3.le_pass = ui.le_pass_yahoo;
		list += t3;
		Transport t4;
		t4.jid = "icq.transport";
		t4.gb = ui.gb_icq;
		t4.le_user = ui.le_user_icq;
		t4.le_pass = ui.le_pass_icq;
		list += t4;

		for(int n = 0; n < list.count(); ++n)
		{
			list[n].le_user->setFixedWidth(160);
			list[n].le_pass->setFixedWidth(160);
			if(list[n].le_nick)
				list[n].le_nick->setFixedWidth(160);

			connect(list[n].le_user, SIGNAL(textEdited(const QString &)), SLOT(textEdited(const QString &)));
			if(list[n].le_nick)
				connect(list[n].le_nick, SIGNAL(textEdited(const QString &)), SLOT(textEdited(const QString &)));
		}

		for(int n = 0; n < list.count(); ++n)
		{
			list[n].avail = false;
			list[n].gb->setEnabled(false);
		}

		ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		connect(ui.buttonBox, SIGNAL(accepted()), SLOT(pb_accept()));
		connect(ui.buttonBox, SIGNAL(rejected()), SLOT(pb_reject()));

		do_set = false;

		if(!pa->isActive())
			return;

		for(int n = 0; n < list.count(); ++n)
		{
			list[n].reg = new JT_Register(pa->client()->rootTask());
			connect(list[n].reg, SIGNAL(finished()), SLOT(reg_finished()));

			list[n].done = false;
			list[n].reg->getForm(list[n].jid);
			list[n].reg->go(true);
		}
	}

	~Private()
	{
		for(int n = 0; n < list.count(); ++n)
			delete list[n].reg;
	}

private slots:
	void pb_accept()
	{
		if(!pa->isActive())
		{
			q->accept();
			return;
		}

		do_register();
	}

	void pb_reject()
	{
		q->reject();
	}

	void do_register()
	{
		do_set = true;

		bool at_least_one = false;
		for(int n = 0; n < list.count(); ++n)
		{
			Transport &trans = list[n];

			// registrations ?
			if(trans.gb->isChecked() && !trans.le_user->text().isEmpty() && (trans.cur_user != trans.le_user->text() || (trans.le_pass->text() != FAKE_PASS && trans.cur_pass != trans.le_pass->text()) || (trans.le_nick && trans.cur_nick != trans.le_nick->text())))
			{
				Jid jid = trans.jid;
				QString user = trans.le_user->text();
				QString pass = trans.le_pass->text();
				QString nick;
				if(trans.le_nick)
					nick = trans.le_nick->text();

				trans.reg = new JT_Register(pa->client()->rootTask());
				connect(trans.reg, SIGNAL(finished()), SLOT(reg_finished()));
				trans.done = false;

				Form form;
				form += FormField("username", user);
				form += FormField("password", pass);
				if(!nick.isEmpty())
					form += FormField("nick", nick);
				form.setJid(jid);
				trans.reg->setForm(form);
				trans.reg->go(true);

				at_least_one = true;
			}
			// unregistrations ?
			else if(!trans.cur_user.isEmpty() && (trans.le_user->text().isEmpty() || !trans.gb->isChecked()))
			{
				Jid jid = trans.jid;
				pa->actionRemove(jid);
			}
		}

		if(at_least_one)
		{
			for(int n = 0; n < list.count(); ++n)
				list[n].gb->setEnabled(false);
			ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		}
		else
			q->accept();
	}

	void reg_finished()
	{
		int at = -1;
		for(int n = 0; n < list.count(); ++n)
		{
			if(list[n].reg == (JT_Register *)sender())
			{
				at = n;
				break;
			}
		}

		Transport &trans = list[at];

		//printf("%s finished\n", qPrintable(trans.jid.full()));
		trans.done = true;

		// get
		if(!do_set)
		{
			if(trans.reg->success())
			{
				trans.avail = true;
				Form form = trans.reg->form();
				foreach(const FormField &i, form)
				{
					if(i.type() == FormField::username)
						trans.cur_user = i.value();
					else if(i.type() == FormField::nick)
						trans.cur_nick = i.value();
					else if(i.type() == FormField::password)
						trans.cur_pass = i.value();
				}
			}
		}
		// set
		else
		{
			// nothing
		}

		trans.reg = 0;

		bool alldone = true;
		for(int n = 0; n < list.count(); ++n)
		{
			if(!list[n].done)
			{
				alldone = false;
				break;
			}
		}

		if(alldone)
		{
			if(do_set)
			{
				q->accept();
				return;
			}

			for(int n = 0; n < list.count(); ++n)
			{
				list[n].gb->setEnabled(true);

				if(list[n].avail && !list[n].cur_user.isEmpty())
				{
					list[n].gb->setChecked(true);
					list[n].le_user->setText(list[n].cur_user);

					if(!list[n].cur_nick.isEmpty() && list[n].le_nick)
						list[n].le_nick->setText(list[n].cur_nick);

					if(!list[n].cur_pass.isEmpty())
						list[n].le_pass->setText(list[n].cur_pass);
					else
						list[n].le_pass->setText(FAKE_PASS);
				}
			}

			ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
		}
	}

	void textEdited(const QString &text)
	{
		Q_UNUSED(text);

		QLineEdit *le = (QLineEdit *)sender();
		int at = -1;
		for(int n = 0; n < list.count(); ++n)
		{
			if((list[n].le_user && list[n].le_user == le) || (list[n].le_nick && list[n].le_nick == le))
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return;

		if(list[at].le_pass->text() == FAKE_PASS)
			list[at].le_pass->setText("");
	}
};

TransportSetupDlg::TransportSetupDlg(PsiAccount *pa, QWidget *parent) :
	QDialog(parent)
{
	d = new Private(this, pa);
}

TransportSetupDlg::~TransportSetupDlg()
{
	delete d;
}

#include "transportsetupdlg.moc"
