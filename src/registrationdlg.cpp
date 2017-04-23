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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QDomElement>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPointer>
#include <QHBoxLayout>
#include <QApplication>
#include <QLabel>

#include "jidutil.h"
#include "psiaccount.h"
#include "registrationdlg.h"
#include "busywidget.h"
#include "common.h"
#include "xdata_widget.h"
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
	QDomElement xdataElement() const;

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

QDomElement JT_XRegister::xdataElement() const
{
	QDomNode n = queryTag(iq()).firstChild();
	for (; !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if (i.isNull())
			continue;

		if (i.attribute("xmlns") == "jabber:x:data")
			return i;
	}

	return QDomElement();
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
	QWidget *gr_form;
	QGridLayout *gr_form_layout;
	Form form;

	QList<QLabel*> lb_field;
	QList<QLineEdit*> le_field;
	XDataWidget *xdata;
};

RegistrationDlg::RegistrationDlg(const Jid &jid, PsiAccount *pa)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->jid = jid;
	d->pa = pa;
	d->pa->dialogRegister(this, d->jid);
	d->jt = 0;
	d->xdata = 0;

	setWindowTitle(tr("Registration: %1").arg(d->jid.full()));

	QVBoxLayout *vb1 = new QVBoxLayout(this);
	vb1->setMargin(4);
	d->lb_top = new QLabel(this);
	d->lb_top->setWordWrap(true);
	d->lb_top->setFrameStyle( QFrame::Panel | QFrame::Sunken );
	d->lb_top->hide();
	vb1->addWidget(d->lb_top);

	d->gr_form = new QWidget(this);
	d->gr_form_layout = new QGridLayout(d->gr_form);
	d->gr_form_layout->setSpacing(4);
	vb1->addWidget(d->gr_form);
	d->gr_form->hide();

	QFrame *line = new QFrame(this);
	line->setFixedHeight(2);
	line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
	vb1->addWidget(line);

	QHBoxLayout *hb1 = new QHBoxLayout;
	vb1->addLayout(hb1);
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

		Q_ASSERT(d->le_field.count() == submitForm.count());
		// import the changes back into the form.
		// the QPtrList of QLineEdits should be in the same order
		for (int i = 0; i < submitForm.count(); ++i) {
			submitForm[i].setValue(d->le_field[i]->text());
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

void RegistrationDlg::setInstructions(const QString& jid, const QString& instructions)
{
	QString str = tr("<b>Registration for \"%1\":</b><br><br>").arg(jid);
	str += TextUtil::plain2rich(instructions);
	d->lb_top->setText(str);
}

/**
 * Returns true if the function was able to successfully process XData.
 */
void RegistrationDlg::processXData(const XData& form)
{
	if (!form.title().isEmpty())
		setWindowTitle(form.title());

	setInstructions(d->jid.full(), form.instructions());

	if (d->xdata)
		delete d->xdata;

	d->xdata = new XDataWidget(d->pa->psi(), d->gr_form, d->pa->client(), d->jid);
	d->gr_form_layout->addWidget(d->xdata); // FIXME
	d->xdata->setForm(form, false);

	d->xdata->show();
}

/**
 * This is used as a fallback when processXData returns true.
 */
void RegistrationDlg::processLegacyForm(const XMPP::Form& form)
{
	setInstructions(d->jid.full(), form.instructions());

	for (Form::ConstIterator it = d->form.begin(); it != d->form.end(); ++it) {
		const FormField &f = *it;

		QLabel *lb = new QLabel(f.fieldName(), d->gr_form);
		QLineEdit *le = new QLineEdit(d->gr_form);
		d->gr_form_layout->addWidget(lb); // FIXME
		d->gr_form_layout->addWidget(le); // FIXME
		if (f.isSecret())
			le->setEchoMode(QLineEdit::Password);
		le->setText(f.value());

		d->lb_field.append(lb);
		d->le_field.append(le);
	}

	if (!d->le_field.isEmpty())
		d->le_field.first()->setFocus();
}

void RegistrationDlg::setData(JT_XRegister* jt)
{
	d->form = jt->form();
	if (jt->hasXData()) {
		processXData(jt->xdata());
	} else {
		processLegacyForm(jt->form());
	}
}

void RegistrationDlg::updateData(JT_XRegister* jt)
{
	if (d->xdata) {
		QDomElement iq = jt->xdataElement();
		if (!iq.isNull()) {
			XData form;
			form.fromXml(iq);
			d->xdata->setForm(form, false);
		}
	}
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
			setData(jt);

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
			QMessageBox::critical(this, tr("Error"), tr("Error submitting registration form.\nReason: %1").arg(jt->statusString()));
			if (jt->statusCode() == 406) {
				// updateData(jt);
			}
			else {
				closeDialogs(this);
				close();
			}
		}
	}
}

#include "registrationdlg.moc"
