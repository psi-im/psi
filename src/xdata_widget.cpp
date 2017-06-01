/*
 * xdata_widget.cpp - a class for displaying jabber:x:data forms
 * Copyright (C) 2003-2004  Michail Pishchagin
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

#include "xdata_widget.h"

#include <QLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QUrl>
#include <QListWidget>
#include <QLineEdit>
#include <QObject>
#include <QTextEdit>
#include <QGridLayout>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSpacerItem>

#include "desktoputil.h"
#include "xmpp_xdata.h"
#include "xmpp_client.h"
#include "xmpp_tasks.h"
#include "psicon.h"
#include "networkaccessmanager.h"

using namespace XMPP;



class XDataMediaWidget : public QLabel
{
	Q_OBJECT

public:
	XDataMediaWidget(XData::Field::MediaUri uri, QSize s,
					 Jid j, XDataWidget *xdw)
		: QLabel(xdw)
		, _xdWidget(xdw)
		, _size(s)
		, _type(uri.mimeType)
	{
		Q_UNUSED(_xdWidget)
		if (uri.uri.startsWith("cid:")) {
			JT_BitsOfBinary *task = new JT_BitsOfBinary(xdw->client()->rootTask());
			connect(task, SIGNAL(finished()), SLOT(bobReceived()));
			task->get(j, uri.uri.mid(4));
			task->go(true);
		} else {
			auto nam = xdw->psi()->networkAccessManager();
			QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(uri.uri)));
			connect(reply, SIGNAL(finished()), SLOT(oobReceived()));
		}
	}

	static QList<XDataMediaWidget*> fromMediaElement(
		XData::Field::MediaElement m,
		Jid j,
		XDataWidget *xdw)
	{
		QList<XDataMediaWidget*> result;
		// simple image filter
		// TODO add support for other formats
		foreach (const XData::Field::MediaUri &uri, m) {
			if (uri.mimeType.startsWith("image")) {
				XDataMediaWidget *mw = new XDataMediaWidget(uri, m.mediaSize(), j, xdw);
				result.append(mw);
			}
		}
		return result;
	}

	static QStringList supportedMedia()
	{
		static QStringList wildcards;
		if (wildcards.isEmpty()) {
			wildcards << "image/*";
		}
		return wildcards;
	}

private:
	void onDataReceived(const QByteArray &data)
	{
		if (!data.isNull()) {
			QPixmap mpix;
			mpix.loadFromData(data);
			if (_size.isEmpty()) {
				setPixmap(mpix);
			} else {
				setPixmap(mpix.scaled(_size));
			}
		}
	}

private slots:
	void bobReceived()
	{
		BoBData &bob = ((JT_BitsOfBinary*)sender())->data();
		onDataReceived(bob.data());
	}

	void oobReceived()
	{
		QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
		onDataReceived(reply->readAll());
		reply->deleteLater();
	}

private:
	XDataWidget* _xdWidget;
	QSize _size;
	QString _type;
};

//----------------------------------------------------------------------------
// XDataField
//----------------------------------------------------------------------------
class XDataField
{
public:
	XDataField(XData::Field f, XDataWidget *w = 0)
	{
		_field = f;
		_xdWidget = w;
	}
	virtual ~XDataField()
	{
	}

	virtual XData::Field field() const
	{
		return _field;
	}

	QString labelText(QString str=": ") const
	{
		QString text = _field.label();
		if ( text.isEmpty() )
			text = _field.var();
		return text + str;
	}

	QString reqText() const
	{
		QString req;
		if ( _field.required() )
			req = "*";
		if ( !_field.desc().isEmpty() ) {
			if ( !req.isEmpty() )
				req += ' ';
			req += "(?)";
		}
		return req;
	}

	virtual bool isValid() const
	{
		return field().isValid();
	}

private:
	XData::Field _field;

protected:
	XDataWidget *_xdWidget;
};

////////////////////////////////////////

class XDataField_Hidden : public XDataField
{
public:
	XDataField_Hidden(XData::Field f, XDataWidget *w)
	: XDataField(f, w)
	{
	}
};

////////////////////////////////////////

class XDataField_Boolean : public XDataField
{
public:
	XDataField_Boolean(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		int row = grid->rowCount();
		bool checked = false;
		if ( f.value().count() ) {
			QString s = f.value().first();
			if ( s == "1" || s == "true" || s == "yes" )
				checked = true;
		}

		QHBoxLayout *layout = new QHBoxLayout;
		QSpacerItem *spacerItem = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

		check = new QCheckBox(xdw);
		check->setChecked(checked);
		layout->addWidget(check);

		QLabel *label = new QLabel(labelText(""), xdw);
		layout->addWidget(label);
		layout->addSpacerItem(spacerItem);

		grid->addLayout(layout, row, 0);

		QLabel *req = new QLabel(reqText(), xdw);
		grid->addWidget(req, row, 1);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			check->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
		QStringList val;
		val << QString( check->isChecked() ? "1" : "0" );
		f.setValue(val);
		return f;
	}

private:
	QCheckBox *check;
};

////////////////////////////////////////

class XDataField_Fixed : public XDataField
{
public:
	XDataField_Fixed(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		int row = grid->rowCount();
		QString text;
		QStringList val = f.value();
		QStringList::Iterator it = val.begin();
		for ( ; it != val.end(); ++it) {
			if ( !text.isEmpty() )
				text += "<br>";
			text += *it;
		}

		QLabel *fixed = new QLabel("<qt>" + text + "<br/></qt>", xdw);
		fixed->setWordWrap(true);
		grid->addWidget(fixed, row, 0);

		if ( !f.desc().isEmpty() ) {
			fixed->setToolTip(f.desc());
		}
	}
};

////////////////////////////////////////

class XDataField_TextSingle : public XDataField
{
public:
	XDataField_TextSingle(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		int row = grid->rowCount();
		XData::Field::MediaElement me = f.mediaElement();
		if (!me.isEmpty()) {
			XDataField *fromField = 0;
			Jid j = xdw->owner();
			if (xdw->registrarType() == "urn:xmpp:captcha"
				&& (fromField = xdw->fieldByVar("from"))) {
				j = Jid(fromField->field().value().value(0));
			}
			QList<XDataMediaWidget*> mediaWidgets = XDataMediaWidget::fromMediaElement(me, j, _xdWidget);
			foreach (XDataMediaWidget* w, mediaWidgets) {
				grid->addWidget(w, row, 0, 1, 3, Qt::AlignCenter);
				row++;
			}
		}

		QString text;
		if ( f.value().count() )
			text = f.value().first();
		QVBoxLayout *layout = new QVBoxLayout;

		QLabel *label = new QLabel(labelText(), xdw);
		label->setWordWrap(true);
		layout->addWidget(label);

		edit = new QLineEdit(xdw);
		edit->setText(text);
		layout->addWidget(edit);

		grid->addLayout(layout, row, 0);

		QLabel *req = new QLabel(reqText(), xdw);
		grid->addWidget(req, row, 1);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			edit->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
		f.setMediaElement(XData::Field::MediaElement());
		QStringList val;
		val << edit->text();
		f.setValue(val);
		return f;
	}

protected:
	QLineEdit *edit;
};

////////////////////////////////////////

class XDataField_TextPrivate : public XDataField_TextSingle
{
public:
	XDataField_TextPrivate(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField_TextSingle(f, grid, xdw)
	{
		edit->setEchoMode(QLineEdit::Password);
	}
};

////////////////////////////////////////

class XDataField_JidSingle : public XDataField_TextSingle
{
public:
	XDataField_JidSingle(XData::Field f, QGridLayout *grid, XDataWidget *w)
	: XDataField_TextSingle(f, grid, w)
	{
		// TODO: add proper validation
	}
};

////////////////////////////////////////

class XDataField_ListSingle : public XDataField
{
public:
	XDataField_ListSingle(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		QHBoxLayout *layout = new QHBoxLayout;

		int row = grid->rowCount();
		QLabel *label = new QLabel(labelText(), xdw);
		label->setWordWrap(true);
		layout->addWidget(label);

		combo = new QComboBox(xdw);
		layout->addWidget(combo);
		combo->setInsertPolicy(QComboBox::NoInsert);

		grid->addLayout(layout, row, 0);

		QString sel;
		if ( !f.value().isEmpty() )
			sel = f.value().first();

		XData::Field::OptionList opts = f.options();
		XData::Field::OptionList::Iterator it = opts.begin();
		for ( ; it != opts.end(); ++it) {
			QString lbl = (*it).label;
			if ( lbl.isEmpty() )
				lbl = (*it).value;

			combo->addItem(lbl);
			if ( (*it).value == sel )
				combo->setCurrentIndex(combo->count()-1);
		}

		QLabel *req = new QLabel(reqText(), xdw);
		grid->addWidget(req, row, 1);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			combo->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		QString lbl = combo->currentText();

		XData::Field f = XDataField::field();
		QStringList val;

		XData::Field::OptionList opts = f.options();
		XData::Field::OptionList::Iterator it = opts.begin();
		for ( ; it != opts.end(); ++it) {
			if ( (*it).label == lbl || (*it).value == lbl ) {
				val << (*it).value;
				break;
			}
		}

		f.setValue(val);
		return f;
	}

private:
	QComboBox *combo;
};

////////////////////////////////////////

class XDataField_ListMulti : public XDataField
{
public:
	XDataField_ListMulti(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		int row = grid->rowCount();
		QLabel *label = new QLabel(labelText(), xdw);
		label->setWordWrap(true);
		grid->addWidget(label, row, 0);

		list = new QListWidget(xdw);
		grid->addWidget(list, row, 1);
		list->setSelectionMode(QAbstractItemView::MultiSelection);

		XData::Field::OptionList opts = f.options();
		XData::Field::OptionList::Iterator it = opts.begin();
		for ( ; it != opts.end(); ++it) {
			QString lbl = (*it).label;
			if ( lbl.isEmpty() )
				lbl = (*it).value;

			QListWidgetItem* item = new QListWidgetItem(lbl,list);

			QStringList val = f.value();
			QStringList::Iterator sit = val.begin();
			for ( ; sit != val.end(); ++sit)
				if ( (*it).label == *sit || (*it).value == *sit )
					list->setItemSelected(item, true);
		}

		QLabel *req = new QLabel(reqText(), xdw);
		grid->addWidget(req, row, 2);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			list->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
		QStringList val;

		for (int i = 0; i < list->count(); i++) {
			QListWidgetItem* item = list->item(i);
			if ( list->isItemSelected(item) ) {
				QString lbl = item->text();
				XData::Field::OptionList opts = f.options();
				XData::Field::OptionList::Iterator it = opts.begin();
				for ( ; it != opts.end(); ++it) {
					if ( (*it).label == lbl || (*it).value == lbl ) {
						val << (*it).value;
						break;
					}
				}
			}
		}

		f.setValue(val);
		return f;
	}

private:
	QListWidget *list;
};

////////////////////////////////////////

class XDataField_TextMulti : public XDataField
{
public:
	XDataField_TextMulti(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField(f, xdw)
	{
		int row = grid->rowCount();
		QHBoxLayout *layout = new QHBoxLayout;

		QLabel *label = new QLabel(labelText(), xdw);
		label->setWordWrap(true);
		layout->addWidget(label);

		edit = new QTextEdit(xdw);
		layout->addWidget(edit);

		QString text;
		QStringList val = f.value();
		QStringList::Iterator it = val.begin();
		for ( ; it != val.end(); ++it) {
			if ( !text.isEmpty() )
				text += '\n';
			text += *it;
		}
		edit->setText(text);

		grid->addLayout(layout, row, 0);

		QLabel *req = new QLabel(reqText(), xdw);
		grid->addWidget(req, row, 1);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			edit->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
		f.setValue( edit->toPlainText().split("\n") );
		return f;
	}

private:
	QTextEdit *edit;
};

////////////////////////////////////////

class XDataField_JidMulti : public XDataField_TextMulti
{
public:
	XDataField_JidMulti(XData::Field f, QGridLayout *grid, XDataWidget *xdw)
	: XDataField_TextMulti(f, grid, xdw)
	{
		// TODO: improve validation
	}
};

//----------------------------------------------------------------------------
// XDataWidget
//----------------------------------------------------------------------------

XDataWidget::XDataWidget(PsiCon *psi, QWidget *parent, XMPP::Client* client, XMPP::Jid owner) :
	QWidget(parent),
    psi_(psi),
	client_(client),
	consistent_(true)
{
	owner_ = owner;
	layout_ = new QVBoxLayout(this);
	layout_->setContentsMargins(0,0,0,0);

}

XDataWidget::~XDataWidget()
{
	qDeleteAll(fields_);
}

PsiCon *XDataWidget::psi() const
{
	return psi_;
}

XMPP::Client* XDataWidget::client() const
{
	return client_;
}

QString XDataWidget::registrarType() const
{
	return registrarType_;
}

XMPP::Jid XDataWidget::owner() const
{
	return owner_;
}

XMPP::Stanza::Error XDataWidget::consistencyError() const
{
	return consistencyError_;
}

void XDataWidget::setInstructions(const QString& instructions)
{
	if (!instructions.isEmpty()) {
		QLabel* l = new QLabel(instructions, this);
		l->setWordWrap(true);
		l->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::LinksAccessibleByMouse);
		connect(l,SIGNAL(linkActivated(const QString&)),SLOT(linkActivated(const QString&)));
		layout_->addWidget(l);
	}
}

void XDataWidget::setForm(const XMPP::XData& d, bool withInstructions)
{
	qDeleteAll(fields_);
	fields_.clear();

	QLayoutItem *child;
	while ((child = layout_->takeAt(0)) != 0) {
		delete child->widget();
		delete child;
	}

	registrarType_ = d.registrarType();
	XData::FieldList fields;
	if (registrarType_ == "urn:xmpp:captcha") {
		QStringList supportedMedia = XDataMediaWidget::supportedMedia();
		QStringList mediaVars;
		mediaVars << "audio_recog" << "ocr" << "picture_q" << "picture_recog"
				  << "speech_q" << "speech_recog" << "video_q" << "video_recog";
		short maxAnswers = 0;
		short requestedAnswers = 0;
		Jid from;
		foreach (const XData::Field &field, d.fields()) {
			if (!field.var().isEmpty()) {
				if (field.var() == "answers") {
					requestedAnswers = field.value().value(0).toInt();
				}
				if (field.var() == "from") {
					from = field.value().value(0);
				}
				if (field.var() == "SHA-256") {
					if (field.required()) {
						consistent_ = false; //sha-256 is not supported atm
						break;
					}
					continue; // unlikely, but who knows
				}
				bool isMedia = mediaVars.contains(field.var());
				if (isMedia || field.var() == "qa") {
					if (isMedia) {
						if (!field.mediaElement().checkSupport(supportedMedia)) {
							if (field.required()) {
								consistent_ = false;
								break;
							}
							continue;
						}
					}
					maxAnswers++;
				}
			}
			fields.append(field);
		}
		if (requestedAnswers > maxAnswers) {
			consistent_ = false;
		}
		if (owner_.domain() != from.domain() || (!owner_.node().isEmpty() &&
												owner_.node() != from.node())) {
			consistent_ = false;
		}
		if (!consistent_) {
			consistencyError_ = Stanza::Error(Stanza::Error::Modify,
											  Stanza::Error::NotAcceptable);
		}
		//TODO check if captcha was sent too late (more than 2 minutes)
	} else {
		fields = d.fields();
	}
	if (withInstructions) {
		setInstructions(d.instructions());
	}
	setFields(fields);
}


XData::FieldList XDataWidget::fields() const
{
	XData::FieldList f;

	for (QList<XDataField*>::ConstIterator it = fields_.begin() ; it != fields_.end(); it ++) {
		f.append( (*it)->field() );
	}

	return f;
}

void XDataWidget::setFields(const XData::FieldList &f)
{
	QWidget *fields = new QWidget(this);
	layout_->addWidget(fields);
	if ( f.count() ) {
		// FIXME
		QGridLayout *grid = new QGridLayout(fields);
		grid->setSpacing(3);

		XData::FieldList::ConstIterator it = f.begin();
		for ( ; it != f.end(); ++it) {
			XDataField *f;
			switch ( (*it).type() ) {
				case XData::Field::Field_Boolean:
					f = new XDataField_Boolean(*it, grid, this);
					break;
				case XData::Field::Field_Fixed:
					f = new XDataField_Fixed(*it, grid, this);
					break;
				case XData::Field::Field_Hidden:
					f = new XDataField_Hidden(*it, this);
					break;
				case XData::Field::Field_JidSingle:
					f = new XDataField_JidSingle(*it, grid, this);
					break;
				case XData::Field::Field_ListMulti:
					f = new XDataField_ListMulti(*it, grid, this);
					break;
				case XData::Field::Field_ListSingle:
					f = new XDataField_ListSingle(*it, grid, this);
					break;
				case XData::Field::Field_TextMulti:
					f = new XDataField_TextMulti(*it, grid, this);
					break;
				case XData::Field::Field_JidMulti:
					f = new XDataField_JidMulti(*it, grid, this);
					break;
				case XData::Field::Field_TextPrivate:
					f = new XDataField_TextPrivate(*it, grid, this);
					break;

				default:
					f = new XDataField_TextSingle(*it, grid, this);
			}
			fields_.append(f);
		}
	}
}

XDataField* XDataWidget::fieldByVar(const QString &var) const
{
	foreach (XDataField* field, fields_) {
		if (field->field().var() == var) {
			return field;
		}
	}
	return 0;
}

void XDataWidget::linkActivated(const QString& link)
{
	DesktopUtil::openUrl(link);
}

#include "xdata_widget.moc"
