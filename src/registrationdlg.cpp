/*
 * registrationdlg.cpp
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include <QDomElement>
#include <QLineEdit>
#include <QMessageBox>
#include <Q3Grid>
#include <Q3PtrList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QLabel>

#include "jidutil.h"
#include "psiaccount.h"
#include "registrationdlg.h"
#include "busywidget.h"
#include "common.h"
#include "xdata_widget.h"
#include "xmpp.h"
#include "xmpp_tasks.h"
#include "textutil.h"
#include "xmpp_xdata.h"
#include "xmpp_xmlcommon.h"

using namespace XMPP;


//----------------------------------------------------------------------------
// JT_XRegister
//----------------------------------------------------------------------------

class JT_XRegister : public JT_Register
{
	Q_OBJECT
public:
	JT_XRegister(Task *parent);

	void setXForm(const Form &frm, const XData &_form);

	bool take(const QDomElement &);
	QDomElement iq() const;

	void onGo();

private:
	QDomElement _iq;
};

JT_XRegister::JT_XRegister( Task *parent )
	: JT_Register( parent )
{
}

bool JT_XRegister::take( const QDomElement &x )
{
	_iq = x;

	return JT_Register::take( x );
}

QDomElement JT_XRegister::iq() const
{
	return _iq;
}

void JT_XRegister::setXForm(const Form &frm, const XData &_form)
{
	JT_Register::setForm( frm );

	_iq = createIQ(doc(), "set", frm.jid().full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	_iq.appendChild(query);

	XData form( _form );
	form.setType( XData::Data_Submit );
	query.appendChild( form.toXml( doc() ) );
}

void JT_XRegister::onGo()
{
	if ( !_iq.isNull() )
		send( _iq );
	else
		JT_Register::onGo();
}

//----------------------------------------------------------------------------
// RegistrationDlg
//----------------------------------------------------------------------------
class RegistrationDlg::Private
{
public:
	Private() {}

	Jid jid;
	PsiAccount *pa;

	QPushButton *pb_close, *pb_reg;
	QPointer<JT_XRegister> jt;
	int type;
	BusyWidget *busy;
	QLabel *lb_top;
	Q3Grid *gr_form;
	Form form;

	Q3PtrList<QLabel> lb_field;
	Q3PtrList<QLineEdit> le_field;
	XDataWidget *xdata;
};

RegistrationDlg::RegistrationDlg(const Jid &jid, PsiAccount *pa)
	: QDialog(0, 0, false)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->jid = jid;
	d->pa = pa;
	d->pa->dialogRegister(this, d->jid);
	d->jt = 0;
	d->xdata = 0;

	d->lb_field.setAutoDelete(true);
	d->le_field.setAutoDelete(true);

	setWindowTitle(tr("Registration: %1").arg(d->jid.full()));

	QVBoxLayout *vb1 = new QVBoxLayout(this, 4);
	d->lb_top = new QLabel(this);
	d->lb_top->setFrameStyle( QFrame::Panel | QFrame::Sunken );
	d->lb_top->hide();
	vb1->addWidget(d->lb_top);

	d->gr_form = new Q3Grid(2, Qt::Horizontal, this);
	d->gr_form->setSpacing(4);
	vb1->addWidget(d->gr_form);
	d->gr_form->hide();

	QFrame *line = new QFrame(this);
	line->setFixedHeight(2);
	line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
	vb1->addWidget(line);

	QHBoxLayout *hb1 = new QHBoxLayout(vb1);
	d->busy = new BusyWidget(this);
	hb1->addWidget(d->busy);
	hb1->addStretch(1);
	d->pb_reg = new QPushButton(tr("&Register"), this);
	d->pb_reg->setDefault(true);
	connect(d->pb_reg, SIGNAL(clicked()), SLOT(doRegSet()));
	hb1->addWidget(d->pb_reg);
	d->pb_close = new QPushButton(tr("&Close"), this);
	connect(d->pb_close, SIGNAL(clicked()), SLOT(close()));
	hb1->addWidget(d->pb_close);

	d->pb_reg->hide();

	doRegGet();
}

RegistrationDlg::~RegistrationDlg()
{
	delete d->jt;
	d->pa->dialogUnregister(this);
	delete d;
}

/*void RegistrationDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void RegistrationDlg::done(int r)
{
	if(d->busy->isActive() && d->type == 1) {
		int n = QMessageBox::information(this, tr("Busy"), tr("<qt>Registration has already been submitted, so closing this window will not prevent the registration from happening.  Do you still wish to close?</qt>"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	QDialog::done(r);
}

void RegistrationDlg::doRegGet()
{
	d->lb_top->setText(tr("Fetching registration form for %1 ...").arg(d->jid.full()));
	d->lb_top->show();
	d->busy->start();

	d->type = 0;
	d->jt = new JT_XRegister(d->pa->client()->rootTask());
	connect(d->jt, SIGNAL(finished()), SLOT(jt_finished()));
	d->jt->getForm(d->jid);
	d->jt->go(true);
}

void RegistrationDlg::doRegSet()
{
	if(!d->pa->checkConnected(this))
		return;

	d->jt = new JT_XRegister(d->pa->client()->rootTask());

	if ( !d->xdata ) {
		Form submitForm = d->form;

		// import the changes back into the form.
		// the QPtrList of QLineEdits should be in the same order
		Q3PtrListIterator<QLineEdit> lit(d->le_field);
		for(Form::Iterator it = submitForm.begin(); it != submitForm.end(); ++it) {
			FormField &f = *it;
			QLineEdit *le = lit.current();
			f.setValue(le->text());
			++lit;
		}

		d->jt->setForm(submitForm);
	}
	else {
		XData form;
		form.setFields( d->xdata->fields() );

		d->jt->setXForm( d->form, form );
	}

	d->gr_form->setEnabled(false);
	d->pb_reg->setEnabled(false);
	d->busy->start();

	d->type = 1;
	connect(d->jt, SIGNAL(finished()), SLOT(jt_finished()));
	d->jt->go(true);
}

void RegistrationDlg::jt_finished()
{
	d->busy->stop();
	d->gr_form->setEnabled(true);
	d->pb_reg->setEnabled(true);
	JT_XRegister *jt = d->jt;
	d->jt = 0;

	if(jt->success()) {
		if(d->type == 0) {
			d->form = jt->form();

			bool useXData = false;
			{
				QDomNode n = queryTag( jt->iq() ).firstChild();
				for( ; !n.isNull(); n = n.nextSibling()) {
					QDomElement i = n.toElement();
					if(i.isNull())
						continue;

					if( i.attribute( "xmlns" ) == "jabber:x:data" ) {
						useXData = true;

						XData form;
						form.fromXml( i );

						if ( !form.title().isEmpty() )
							setWindowTitle( form.title() );

						QString str = tr("<b>Registration for \"%1\":</b><br><br>").arg(d->jid.full());
						str += TextUtil::plain2rich( form.instructions() );
						d->lb_top->setText(str);

						d->xdata = new XDataWidget( d->gr_form );
						d->xdata->setFields( form.fields() );

						d->xdata->show();

						break;
					}
				}
			}

			if ( !useXData ) {
				QString str = tr("<b>Registration for \"%1\":</b><br><br>").arg(d->jid.full());
				str += TextUtil::plain2rich(d->form.instructions());
				d->lb_top->setText(str);
				d->lb_top->setFixedWidth(300);

				for(Form::ConstIterator it = d->form.begin(); it != d->form.end(); ++it) {
					const FormField &f = *it;

					QLabel *lb = new QLabel(f.fieldName(), d->gr_form);
					QLineEdit *le = new QLineEdit(d->gr_form);
					if(f.isSecret())
						le->setEchoMode(QLineEdit::Password);
					le->setText(f.value());

					d->lb_field.append(lb);
					d->le_field.append(le);
				}

				if (!d->le_field.isEmpty())
					d->le_field.first()->setFocus();
			}

			d->gr_form->show();
			d->pb_reg->show();
			show();

			qApp->processEvents();
			resize(sizeHint());
		}
		else {
			closeDialogs(this);
			QMessageBox::information(this, tr("Success"), tr("Registration successful."));
			close();
		}
	}
	else {
		if(d->type == 0) {
			QMessageBox::critical(this, tr("Error"), tr("Unable to retrieve registration form.\nReason: %1").arg(jt->statusString()));
			close();
		}
		else {
			closeDialogs(this);
			QMessageBox::critical(this, tr("Error"), tr("Error submitting registration form.\nReason: %1").arg(jt->statusString()));
			close();
		}
	}
}

#include "registrationdlg.moc"
