/*
 * searchdlg.cpp
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
#include <QPointer>
#include <QLineEdit>
#include <QMessageBox>

#include "jidutil.h"
#include "psiaccount.h"
#include "common.h"
#include "xdata_widget.h"
#include "xmpp_tasks.h"
#include "xmpp_xdata.h"
#include "xmpp_xmlcommon.h"
#include "textutil.h"
#include "searchdlg.h"

using namespace XMPP;

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
	Private(SearchDlg* _dlg)
		: dlg(_dlg)
	{}

	struct NickAndJid {
		QString nick;
		XMPP::Jid jid;
	};

	QList<NickAndJid> selectedNicksAndJids() const
	{
		QList<NickAndJid> result;

		int jid;
		int nick;
		if (!xdata) {
			jid  = 4;
			nick = 0;
		}
		else {
			jid  = 0;
			nick = 0;

			int i = 0;
			QList<XData::ReportField>::ConstIterator it = xdata_form.report().begin();
			for (; it != xdata_form.report().end(); ++it, ++i) {
				QString name = (*it).name;
				if (name == "jid")
					jid = i;

				if (name == "nickname" || name == "nick" || name == "title")
					nick = i;
			}
		}

		foreach(QTreeWidgetItem* i, dlg->lv_results->selectedItems()) {
			NickAndJid nickJid;
			nickJid.jid  = XMPP::Jid(i->text(jid));
			nickJid.nick = i->text(nick);
			result << nickJid;
		}

		return result;
	}

	SearchDlg* dlg;
	PsiAccount *pa;
	Jid jid;
	Form form;
	BusyWidget *busy;
	QPointer<JT_XSearch> jt;
	QWidget *gr_form;
	QGridLayout *gr_form_layout;
	int type;

	QList<QLabel*> lb_field;
	QList<QLineEdit*> le_field;
	XDataWidget *xdata;
	XData xdata_form;
	QScrollArea *scrollArea;

};

SearchDlg::SearchDlg(const Jid &jid, PsiAccount *pa)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private(this);
	setupUi(this);
	setModal(false);
	d->pa = pa;
	d->jid = jid;
	d->pa->dialogRegister(this, d->jid);
	d->jt = 0;
	d->xdata = 0;
	d->scrollArea = scrollArea;


	setWindowTitle(windowTitle().arg(d->jid.full()));

	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	d->busy = busy;

	d->gr_form = new QWidget(gb_search);
	d->gr_form_layout = new QGridLayout(d->gr_form);
	d->gr_form_layout->setSpacing(0);
	d->scrollArea->setWidget(d->gr_form);
	d->gr_form->hide();

	pb_add->setEnabled(false);
	pb_info->setEnabled(false);
	pb_stop->setEnabled(false);
	pb_search->setEnabled(false);

	connect(lv_results, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
	connect(lv_results, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(itemActivated(QTreeWidgetItem*, int)));
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
	QTreeWidgetItem* lvi = new QTreeWidgetItem(lv_results);
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

		Q_ASSERT(submitForm.count() == d->le_field.count());
		// import the changes back into the form.
		// the QPtrList of QLineEdits should be in the same order
		for (int i = 0; i < submitForm.count(); ++i) {
			submitForm[i].setValue(d->le_field[i]->text());
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

						QString str = TextUtil::plain2rich( form.instructions() );
						lb_instructions->setText(str);

						d->xdata = new XDataWidget(d->pa->psi(), d->gr_form, d->pa->client(), d->form.jid() );
						d->gr_form_layout->addWidget(d->xdata); // FIXME
						d->xdata->setForm( form, false );

						d->xdata->show();

						break;
					}
				}
			}

			if ( !useXData ) {
				QString str = TextUtil::plain2rich(d->form.instructions());
				lb_instructions->setText(str);

				for(Form::ConstIterator it = d->form.begin(); it != d->form.end(); ++it) {
					const FormField &f = *it;

					QLabel *lb = new QLabel(f.fieldName(), d->gr_form);
					QLineEdit *le = new QLineEdit(d->gr_form);
					d->gr_form_layout->addWidget(lb); // FIXME
					d->gr_form_layout->addWidget(le); // FIXME
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
			lv_results->setUpdatesEnabled(false);

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

				QStringList header_labels;
				foreach(XData::ReportField report, form.report()) {
					header_labels << report.label;
				}

				lv_results->clear();
				lv_results->setColumnCount(0);
				lv_results->setHeaderLabels(header_labels);

				foreach(XData::ReportItem ri, form.reportItems()) {
					int i = 0;
					QTreeWidgetItem* lvi = new QTreeWidgetItem(lv_results);
					foreach(XData::ReportField report, form.report()) {
						lvi->setText(i++, ri[report.name]);
					}
				}

				d->xdata_form = form;
			}

			for (int i = 0; i < lv_results->columnCount(); ++i) {
				lv_results->resizeColumnToContents(i);
				lv_results->setColumnWidth(i, qMin(lv_results->columnWidth(i), 300));
			}

			lv_results->sortByColumn(0, Qt::AscendingOrder);
			lv_results->setUpdatesEnabled(true);
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
	bool enable = !lv_results->selectedItems().isEmpty();
	pb_add->setEnabled(enable);
	pb_info->setEnabled(enable);
}

void SearchDlg::itemActivated(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(item);
	Q_UNUSED(column);
	doInfo();
}

void SearchDlg::doAdd()
{
	QList<Private::NickAndJid> nicksAndJids = d->selectedNicksAndJids();
	if (nicksAndJids.isEmpty())
		return;

	foreach(Private::NickAndJid nickJid, nicksAndJids)
		emit add(nickJid.jid, nickJid.nick, QStringList(), true);

	if (nicksAndJids.count() > 1) {
		QMessageBox::information(this,
		                         tr("Add User: Success"),
		                         tr("Added %n users to your roster.", "", nicksAndJids.count()));
	}
	else {
		QMessageBox::information(this,
		                         tr("Add User: Success"),
		                         tr("Added %1 to your roster.").arg(
		                             JIDUtil::nickOrJid(nicksAndJids.first().nick,
		                                                nicksAndJids.first().jid.full()
		                                               )));
	}
}

void SearchDlg::doInfo()
{
	QList<Private::NickAndJid> nicksAndJids = d->selectedNicksAndJids();
	if (nicksAndJids.isEmpty())
		return;

	foreach(Private::NickAndJid nickJid, nicksAndJids)
		emit aInfo(nickJid.jid);
}

#include "searchdlg.moc"
