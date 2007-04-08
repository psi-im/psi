/*
 * servicesdlg.cpp - a dialog for browsing Jabber services
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

#include "servicesdlg.h"

#include <qpushbutton.h>
#include <qcombobox.h>
#include <qstringlist.h>
#include <q3listbox.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <q3grid.h>
#include <qpointer.h>
#include <q3listview.h>
#include <q3groupbox.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <Q3PtrList>
#include <QHBoxLayout>
#include <QList>
#include <QVBoxLayout>
#include "psicon.h"
#include "psiaccount.h"
#include "im.h"
#include "xmpp_tasks.h"
#include "common.h"
#include "busywidget.h"
#include "iconwidget.h"
#include "xdata_widget.h"
#include "xmpp_xdata.h"
#include "jidutil.h"
#include "xmpp_xmlcommon.h"

using namespace XMLHelper;


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
:QDialog(0, 0, false, Qt::WDestructiveClose)
{
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
						str += plain2rich( form.instructions() );
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
				str += plain2rich(d->form.instructions());
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

//----------------------------------------------------------------------------
// JT_XSearch
//----------------------------------------------------------------------------

class JT_XSearch : public JT_Search
{
	Q_OBJECT
public:
	JT_XSearch(Task *parent);

	void setForm(const Form &frm, const XData &_form);

	bool take(const QDomElement &);
	QDomElement iq() const;

	void onGo();

private:
	QDomElement _iq;
};

JT_XSearch::JT_XSearch( Task *parent )
	: JT_Search( parent )
{
}

bool JT_XSearch::take( const QDomElement &x )
{
	_iq = x;

	return JT_Search::take( x );
}

QDomElement JT_XSearch::iq() const
{
	return _iq;
}

void JT_XSearch::setForm(const Form &frm, const XData &_form)
{
	JT_Search::set( frm );

	_iq = createIQ(doc(), "set", frm.jid().full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:search");
	_iq.appendChild(query);

	XData form( _form );
	form.setType( XData::Data_Submit );
	query.appendChild( form.toXml( doc() ) );
}

void JT_XSearch::onGo()
{
	if ( !_iq.isNull() )
		send( _iq );
	else
		JT_Search::onGo();
}

//----------------------------------------------------------------------------
// SearchDlg
//----------------------------------------------------------------------------
class SearchDlg::Private
{
public:
	Private() {}

	PsiAccount *pa;
	Jid jid;
	Form form;
	BusyWidget *busy;
	QPointer<JT_XSearch> jt;
	Q3Grid *gr_form;
	int type;

	Q3PtrList<QLabel> lb_field;
	Q3PtrList<QLineEdit> le_field;
	XDataWidget *xdata;
	XData xdata_form;
};

SearchDlg::SearchDlg(const Jid &jid, PsiAccount *pa) : QDialog(0, Qt::WDestructiveClose)
{
	d = new Private;
	setupUi(this);
	setModal(false);
	d->pa = pa;
	d->jid = jid;
	d->pa->dialogRegister(this, d->jid);
	d->jt = 0;
	d->xdata = 0;

	setWindowTitle(caption().arg(d->jid.full()));

	d->busy = busy;

	d->gr_form = new Q3Grid(2, Qt::Horizontal, gb_search);
	d->gr_form->setSpacing(4);
	replaceWidget(lb_form, d->gr_form);
	d->gr_form->hide();

	pb_add->setEnabled(false);
	pb_info->setEnabled(false);
	pb_stop->setEnabled(false);
	pb_search->setEnabled(false);

	lv_results->setMultiSelection(true);
	lv_results->setSelectionMode( Q3ListView::Extended );
	connect(lv_results, SIGNAL(selectionChanged()), SLOT(selectionChanged()));
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_search, SIGNAL(clicked()), SLOT(doSearchSet()));
	connect(pb_stop, SIGNAL(clicked()), SLOT(doStop()));
	connect(pb_add, SIGNAL(clicked()), SLOT(doAdd()));
	connect(pb_info, SIGNAL(clicked()), SLOT(doInfo()));

	resize(600,440);

	doSearchGet();
}

SearchDlg::~SearchDlg()
{
	delete d->jt;
	d->pa->dialogUnregister(this);
	delete d;
}

/*void SearchDlg::localUpdate(const JabRosterEntry &e)
{
	int oldstate = localStatus;
	localStatus = e.localStatus();

	if(localStatus == STATUS_OFFLINE) {
		if(isBusy) {
			busy->stop();
			isBusy = FALSE;
		}

		pb_search->setEnabled(FALSE);
		pb_stop->setEnabled(FALSE);
		clear();
	}
	else {
		// coming online?
		if(oldstate == STATUS_OFFLINE) {
			pb_search->setEnabled(TRUE);
		}
	}
}*/

void SearchDlg::addEntry(const QString &jid, const QString &nick, const QString &first, const QString &last, const QString &email)
{
	Q3ListViewItem *lvi = new Q3ListViewItem(lv_results);
	lvi->setText(0, nick);
	lvi->setText(1, first);
	lvi->setText(2, last);
	lvi->setText(3, email);
	lvi->setText(4, jid);
}

void SearchDlg::doSearchGet()
{
	lb_instructions->setText(tr("<qt>Fetching search form for %1 ...</qt>").arg(d->jid.full()));
	d->busy->start();

	d->type = 0;
	d->jt = new JT_XSearch(d->pa->client()->rootTask());
	connect(d->jt, SIGNAL(finished()), SLOT(jt_finished()));
	d->jt->get(d->jid);
	d->jt->go(true);
}

void SearchDlg::doSearchSet()
{
	if(d->busy->isActive())
		return;

	if(!d->pa->checkConnected(this))
		return;

	d->jt = new JT_XSearch(d->pa->client()->rootTask());

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

		d->jt->set(submitForm);
	}
	else {
		XData form;
		form.setFields( d->xdata->fields() );

		d->jt->setForm( d->form, form );
	}

	clear();

	pb_search->setEnabled(false);
	pb_stop->setEnabled(true);
	d->gr_form->setEnabled(false);
	d->busy->start();

	d->type = 1;
	connect(d->jt, SIGNAL(finished()), SLOT(jt_finished()));
	d->jt->go(true);
}

void SearchDlg::jt_finished()
{
	d->busy->stop();
	JT_XSearch *jt = d->jt;
	d->jt = 0;

	if(d->type == 1) {
		d->gr_form->setEnabled(true);
		pb_search->setEnabled(true);
		pb_stop->setEnabled(false);
	}

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

						//if ( !form.title().isEmpty() )
						//	setWindowTitle( form.title() );

						QString str = plain2rich( form.instructions() );
						lb_instructions->setText(str);

						d->xdata = new XDataWidget( d->gr_form );
						d->xdata->setFields( form.fields() );

						d->xdata->show();

						break;
					}
				}
			}

			if ( !useXData ) {
				QString str = plain2rich(d->form.instructions());
				lb_instructions->setText(str);

				for(Form::ConstIterator it = d->form.begin(); it != d->form.end(); ++it) {
					const FormField &f = *it;

					QLabel *lb = new QLabel(f.fieldName(), d->gr_form);
					QLineEdit *le = new QLineEdit(d->gr_form);
					if(f.isSecret())
						le->setEchoMode(QLineEdit::Password);
					le->setText(f.value());

					d->lb_field.append(lb);
					d->le_field.append(le);

					connect(le, SIGNAL(returnPressed()), this, SLOT(doSearchSet()));
				}
			}

			d->gr_form->show();
			pb_search->setEnabled(true);
			show();

			qApp->processEvents();
			resize(sizeHint());
		}
		else {
			if ( !d->xdata ) {
				const QList<SearchResult> &list = jt->results();
				if(list.isEmpty())
					QMessageBox::information(this, tr("Search Results"), tr("Search returned 0 results."));
				else {
					for(QList<SearchResult>::ConstIterator it = list.begin(); it != list.end(); ++it) {
						const SearchResult &r = *it;
						addEntry(r.jid().full(), r.nick(), r.first(), r.last(), r.email());
					}
				}
			}
			else {
				XData form;
				QDomNode n = queryTag( jt->iq() ).firstChild();
				for( ; !n.isNull(); n = n.nextSibling()) {
					QDomElement i = n.toElement();
					if(i.isNull())
						continue;

					if( i.attribute( "xmlns" ) == "jabber:x:data" ) {
						form.fromXml( i );
						break;
					}
				}

				while ( lv_results->columns() )
					lv_results->removeColumn( 0 );

				QList<XData::ReportField>::ConstIterator it = form.report().begin();
				for ( ; it != form.report().end(); ++it ) {
					lv_results->addColumn( ( *it ).label );
				}

				QList<XData::ReportItem>::ConstIterator iit = form.reportItems().begin();
				for ( ; iit != form.reportItems().end(); ++iit ) {
					Q3ListViewItem *lvi = new Q3ListViewItem(lv_results);

					int i = 0;
					it = form.report().begin();
					for ( ; it != form.report().end(); ++it ) {
						QString name = ( *it ).name;
						lvi->setText( i++, ( *iit )[name] );
					}
				}

				d->xdata_form = form;
			}
		}
	}
	else {
		if(d->type == 0) {
			QMessageBox::critical(this, tr("Error"), tr("Unable to retrieve search form.\nReason: %1").arg(jt->statusString()));
			close();
		}
		else {
			QMessageBox::critical(this, tr("Error"), tr("Error retrieving search results.\nReason: %1").arg(jt->statusString()));
		}
	}
}

void SearchDlg::clear()
{
	lv_results->clear();
	pb_add->setEnabled(false);
	pb_info->setEnabled(false);
}

void SearchDlg::doStop()
{
	if(!d->busy->isActive())
		return;

	delete d->jt;
	d->jt = 0;

	d->busy->stop();
	d->gr_form->setEnabled(true);
	pb_search->setEnabled(true);
	pb_stop->setEnabled(false);
}

void SearchDlg::selectionChanged()
{
	int d = 0;
	Q3ListViewItem *lastChild = lv_results->firstChild();

	if(!lastChild) {
		pb_add->setEnabled(false);
		pb_info->setEnabled(false);
		return;
	}

	if( lastChild->isSelected() ) {
		pb_add->setEnabled(true);
		pb_info->setEnabled(true);
	}
	d++;

	if ( lastChild ) {
		while ( lastChild->nextSibling() ) {
			lastChild = lastChild->nextSibling();
			if( lastChild->isSelected() ) {
				pb_add->setEnabled(true);
				pb_info->setEnabled(true);
			}
 			d++;
		}
	}
}

void SearchDlg::doAdd()
{
	Q3ListViewItem *i = lv_results->firstChild();
	QString name;

	if(!i)
		return;

	int jid;
	int nick;
	if ( !d->xdata ) {
		jid  = 4;
		nick = 0;
	}
	else {
		jid = 0;
		nick = 0;

		int i = 0;
		QList<XData::ReportField>::ConstIterator it = d->xdata_form.report().begin();
		for ( ; it != d->xdata_form.report().end(); ++it, ++i ) {
			QString name = ( *it ).name;
			if ( name == "jid" )
				jid = i;

			if ( name == "nickname" || name == "nick" )
				nick = i;
		}
	}

	int d = 0;

	if( i->isSelected() ) {
		name = JIDUtil::nickOrJid(i->text(nick), i->text(jid));
		add(Jid(i->text(jid)), i->text(nick), QStringList(), true);
		d++;
	}

	if( i ) {
		while( i->nextSibling() ) {
			i = i->nextSibling();
			if( i->isSelected() ) {
				name = JIDUtil::nickOrJid(i->text(nick), i->text(jid));
				add(Jid(i->text(jid)), i->text(nick), QStringList(), true);
				d++;
			}
		}
	}

	if( d==1 )
		QMessageBox::information(this, tr("Add User: Success"), tr("Added %1 to your roster.").arg(name));
	else
		QMessageBox::information(this, tr("Add User: Success"), tr("Added %1 users to your roster.").arg(d));
}

void SearchDlg::doInfo()
{
	Q3ListViewItem *i = lv_results->firstChild();
	QString name;

	if(!i)
		return;

	int jid;
	if ( !d->xdata ) {
		jid  = 4;
	}
	else {
		jid = 0;

		int i = 0;
		QList<XData::ReportField>::ConstIterator it = d->xdata_form.report().begin();
		for ( ; it != d->xdata_form.report().end(); ++it, ++i ) {
			QString name = ( *it ).name;
			if ( name == "jid" )
				jid = i;
		}
	}

	if( i->isSelected() ) {
		aInfo(Jid(i->text(jid)));
	}

	if( i ) {
		while( i->nextSibling() ) {
			i = i->nextSibling();
			if( i->isSelected() ) {
				aInfo(Jid(i->text(jid)));
			}
		}
	}
}

#include "servicesdlg.moc"
