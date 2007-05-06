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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include "desktoputil.h"
#include "xmpp_xdata.h"

using namespace XMPP;

//----------------------------------------------------------------------------
// XDataField
//----------------------------------------------------------------------------
class XDataField
{
public:
	XDataField(XData::Field f)
	{
		_field = f;
	}
	virtual ~XDataField()
	{
	}

	virtual XData::Field field() const
	{
		return _field;
	}

	QString labelText() const
	{
		QString text = _field.label();
		if ( text.isEmpty() )
			text = _field.var();
		return text + ": ";
	}

	QString reqText() const
	{
		QString req;
		if ( _field.required() )
			req = "*";
		if ( !_field.desc().isEmpty() ) {
			if ( !req.isEmpty() )
				req += " ";
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
};

////////////////////////////////////////

class XDataField_Hidden : public XDataField
{
public:
	XDataField_Hidden(XData::Field f)
	: XDataField(f)
	{
	}
};

////////////////////////////////////////

class XDataField_Boolean : public XDataField
{
public:
	XDataField_Boolean(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		bool checked = false;
		if ( f.value().count() ) {
			QString s = f.value().first();
			if ( s == "1" || s == "true" || s == "yes" )
				checked = true;
		}

		QLabel *label = new QLabel(labelText(), parent);
		grid->addWidget(label, row, 0);

		check = new QCheckBox(parent);
		check->setChecked(checked);
		grid->addWidget(check, row, 1);

		QLabel *req = new QLabel(reqText(), parent);
		grid->addWidget(req, row, 2);

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
	XDataField_Fixed(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		QString text;
		QStringList val = f.value();
		QStringList::Iterator it = val.begin();
		for ( ; it != val.end(); ++it) {
			if ( !text.isEmpty() )
				text += "<br>";
			text += *it;
		}

		QLabel *fixed = new QLabel("<qt>" + text + "</qt>", parent);
		grid->addMultiCellWidget(fixed, row, row, 0, 2);

		if ( !f.desc().isEmpty() ) {
			fixed->setToolTip(f.desc());
		}
	}
};

////////////////////////////////////////

class XDataField_TextSingle : public XDataField
{
public:
	XDataField_TextSingle(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		QString text;
		if ( f.value().count() )
			text = f.value().first();

		QLabel *label = new QLabel(labelText(), parent);
		grid->addWidget(label, row, 0);

		edit = new QLineEdit(parent);
		edit->setText(text);
		grid->addWidget(edit, row, 1);

		QLabel *req = new QLabel(reqText(), parent);
		grid->addWidget(req, row, 2);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			edit->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
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
	XDataField_TextPrivate(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField_TextSingle(f, grid, row, parent)
	{
		edit->setEchoMode(QLineEdit::Password);
	}
};

////////////////////////////////////////

class XDataField_JidSingle : public XDataField_TextSingle
{
public:
	XDataField_JidSingle(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField_TextSingle(f, grid, row, parent)
	{
		// TODO: add proper validation
	}
};

////////////////////////////////////////

class XDataField_ListSingle : public XDataField
{
public:
	XDataField_ListSingle(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		QLabel *label = new QLabel(labelText(), parent);
		grid->addWidget(label, row, 0);

		combo = new QComboBox(parent);
		grid->addWidget(combo, row, 1);
		combo->setInsertionPolicy(QComboBox::NoInsertion);

		QString sel;
		if ( !f.value().isEmpty() )
			sel = f.value().first();

		XData::Field::OptionList opts = f.options();
		XData::Field::OptionList::Iterator it = opts.begin();
		for ( ; it != opts.end(); ++it) {
			QString lbl = (*it).label;
			if ( lbl.isEmpty() )
				lbl = (*it).value;

			combo->insertItem(lbl);
			if ( (*it).value == sel )
				combo->setCurrentText( lbl );
		}

		QLabel *req = new QLabel(reqText(), parent);
		grid->addWidget(req, row, 2);

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
	XDataField_ListMulti(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		QLabel *label = new QLabel(labelText(), parent);
		grid->addWidget(label, row, 0);

		list = new QListWidget(parent);
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

		QLabel *req = new QLabel(reqText(), parent);
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
	XDataField_TextMulti(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField(f)
	{
		QLabel *label = new QLabel(labelText(), parent);
		grid->addWidget(label, row, 0);

		edit = new QTextEdit(parent);
		grid->addWidget(edit, row, 1);

		QString text;
		QStringList val = f.value();
		QStringList::Iterator it = val.begin();
		for ( ; it != val.end(); ++it) {
			if ( !text.isEmpty() )
				text += "\n";
			text += *it;
		}
		edit->setText(text);

		QLabel *req = new QLabel(reqText(), parent);
		grid->addWidget(req, row, 2);

		if ( !f.desc().isEmpty() ) {
			label->setToolTip(f.desc());
			edit->setToolTip(f.desc());
			req->setToolTip(f.desc());
		}
	}

	XData::Field field() const
	{
		XData::Field f = XDataField::field();
		f.setValue( QStringList::split("\n", edit->text(), true) );
		return f;
	}

private:
	QTextEdit *edit;
};

////////////////////////////////////////

class XDataField_JidMulti : public XDataField_TextMulti
{
public:
	XDataField_JidMulti(XData::Field f, QGridLayout *grid, int row, QWidget *parent)
	: XDataField_TextMulti(f, grid, row, parent)
	{
		// TODO: improve validation
	}
};

//----------------------------------------------------------------------------
// XDataWidget
//----------------------------------------------------------------------------

XDataWidget::XDataWidget(QWidget *parent, const char *name)
: QWidget(parent, name)
{
}

XDataWidget::~XDataWidget()
{
}

void XDataWidget::setInstructions(const QString& instructions)
{
	instructions_ = instructions;
}

void XDataWidget::setForm(const XMPP::XData& d) 
{
	setInstructions(d.instructions());
	setFields(d.fields());
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
	fields_.clear();

	// delete all child widgets
	QObjectList objlist = queryList();
	while (!objlist.isEmpty()) {
		delete objlist.takeFirst();
	}


	QVBoxLayout* vert = new QVBoxLayout(this);
	if (!instructions_.isEmpty()) {
		QLabel* l = new QLabel(instructions_, this);
		l->setWordWrap(true);
		l->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::LinksAccessibleByMouse);
		connect(l,SIGNAL(linkActivated(const QString&)),SLOT(linkActivated(const QString&)));
		vert->addWidget(l);
	}
	QWidget *fields = new QWidget(this);
	vert->addWidget(fields);
	if ( f.count() ) {
		QGridLayout *grid = new QGridLayout(fields, 3, f.count(), 0, 3);

		int row = 0;
		XData::FieldList::ConstIterator it = f.begin();
		for ( ; it != f.end(); ++it, ++row) {
			XDataField *f;
			switch ( (*it).type() ) {
				case XData::Field::Field_Boolean:
					f = new XDataField_Boolean(*it, grid, row, this);
					break;
				case XData::Field::Field_Fixed:
					f = new XDataField_Fixed(*it, grid, row, this);
					break;
				case XData::Field::Field_Hidden:
					f = new XDataField_Hidden(*it);
					break;
				case XData::Field::Field_JidSingle:
					f = new XDataField_JidSingle(*it, grid, row, this);
					break;
				case XData::Field::Field_ListMulti:
					f = new XDataField_ListMulti(*it, grid, row, this);
					break;
				case XData::Field::Field_ListSingle:
					f = new XDataField_ListSingle(*it, grid, row, this);
					break;
				case XData::Field::Field_TextMulti:
					f = new XDataField_TextMulti(*it, grid, row, this);
					break;
				case XData::Field::Field_JidMulti:
					f = new XDataField_JidMulti(*it, grid, row, this);
					break;
				case XData::Field::Field_TextPrivate:
					f = new XDataField_TextPrivate(*it, grid, row, this);
					break;

				default:
					f = new XDataField_TextSingle(*it, grid, row, this);
			}
			fields_.append(f);
		}
	}
}

void XDataWidget::linkActivated(const QString& link)
{
	DesktopUtil::openUrl(link);
}
