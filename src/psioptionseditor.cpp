
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QVariant>
#include <QMessageBox>
#include <QTextDocument>

#include "psioptionseditor.h"
#include "psioptions.h"
#include "common.h"
#include "iconset.h"

#include "ui_optioneditor.h"

class OptionEditor : public QDialog, protected Ui_OptionEditor {
	Q_OBJECT
public:
	OptionEditor(bool new_, QString name, QVariant value);

signals:
	void commit(QString name, QVariant value);

protected slots:
	void finished();
protected:
	struct supportedType {
		const char *name;
		QVariant::Type typ;
	};
	static supportedType supportedTypes[];
};

OptionEditor::supportedType OptionEditor::supportedTypes[] = {
	{"bool", QVariant::Bool},
	{"int", QVariant::Int},
	{"QKeySequence", QVariant::KeySequence},
	{"QSize", QVariant::Size},
	{"QString", QVariant::String},
//	{"QStringList", QVariant::StringList},  does't work
	{0, QVariant::Invalid}};


OptionEditor::OptionEditor(bool new_, QString name_, QVariant value_)
{
	setupUi(this);
	
	if (new_) {
		setWindowTitle(tr("Psi: Option Editor"));
	} else {
		setWindowTitle(tr("Psi: Edit Option %1").arg(name_));
	}
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(finished()));
	for (int i=0; supportedTypes[i].name; i++) {
		cb_typ->addItem(supportedTypes[i].name);
	}
	le_option->setText(name_);
	if (!new_) {
		le_option->setReadOnly(false);
		lb_comment->setText(PsiOptions::instance()->getComment(name_));
	}
	if (value_.isValid()) {
		bool ok=false;
		for (int i=0; supportedTypes[i].name; i++) {
			if (value_.type() == supportedTypes[i].typ) {
				cb_typ->setCurrentIndex(i);
				le_value->setText(value_.toString());
				ok = true;
				break;
			}
		}
		if (!ok) {
			QMessageBox::critical(this, tr("Psi: Option Editor"),
                   tr("Can't edit this type of setting, sorry."), QMessageBox::Close);
			deleteLater();
		}
	}
	resize(sizeHint());
	show();
}

void OptionEditor::finished()
{	
	QString option = le_option->text();
	if (option.isEmpty() || option.endsWith(".") || option.contains("..") || !PsiOptions::isValidName(option)) {
		QMessageBox::critical(this, tr("Psi: Option Editor"),
			tr("Please enter option name.\n\n"
			"Option names may not be empty, end in '.' or contain '..'."), QMessageBox::Close);
		return;
	}
	QVariant strval(le_value->text());
	QVariant::Type type = supportedTypes[cb_typ->currentIndex()].typ;
	QVariant newval = strval;
	newval.convert(type);
	PsiOptions::instance()->setOption(option, newval);

	accept();
}

PsiOptionsEditor::PsiOptionsEditor(QWidget *parent)
		: QWidget(parent)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

	o_ = PsiOptions::instance();
	tm_ = new OptionsTreeModel(o_);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setSpacing(0);
	layout->setMargin(0);
	
	tv_ = new QTreeView(this);
	tv_->setModel(tm_);
	tv_->setAlternatingRowColors(true);
	layout->addWidget(tv_);
	tv_->setColumnHidden(1, true);
	tv_->setColumnHidden(3, true);
	tv_->resizeColumnToContents(0);
	tv_colWidth = tv_->columnWidth(0);
	
	QHBoxLayout *infoLine = new QHBoxLayout;
	layout->addLayout(infoLine);
	lb_path = new QLabel(this);
	lb_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
	lb_path->setToolTip(tr("Full name of the currently selected option."));
	infoLine->addWidget(lb_path);
	
	infoLine->addStretch(1);
	
	lb_type = new QLabel(this);
	lb_type->setText(tr("(no selection)"));
	lb_type->setTextFormat(Qt::RichText);

	infoLine->addWidget(lb_type);

	
	
	lb_comment = new QLabel(this);
	lb_comment->setTextInteractionFlags(Qt::TextSelectableByMouse);
	lb_comment->setText("   ");
	lb_comment->setWordWrap(true);
	lb_comment->setTextFormat(Qt::PlainText);

	layout->addWidget(lb_comment);

	QHBoxLayout* buttonLine = new QHBoxLayout;
	layout->addLayout(buttonLine);

	cb_ = new QCheckBox(this);
	cb_->setText(tr("Flat"));
	cb_->setToolTip(tr("Display all options as a flat list."));
	cb_->setProperty("isOption", false);
	connect(cb_,SIGNAL(toggled(bool)),tm_,SLOT(setFlat(bool)));
	buttonLine->addWidget(cb_);

	buttonLine->addStretch(1);

	if (1) { // FIXME
		pb_delete = new QPushButton(tr("Delete"), this);
		buttonLine->addWidget(pb_delete);
		connect(pb_delete, SIGNAL(clicked()), SLOT(deleteit()));
	}
	
	pb_edit = new QPushButton(tr("Edit..."), this);
	buttonLine->addWidget(pb_edit);
	connect(pb_edit, SIGNAL(clicked()), SLOT(edit()));
	
	pb_new = new QPushButton(tr("Add..."), this);
	buttonLine->addWidget(pb_new);
	connect(pb_new, SIGNAL(clicked()), SLOT(add()));

	
	if (parent) {
		pb_detach = new QToolButton(this);
		pb_detach->setIcon(IconsetFactory::iconPixmap("psi/advanced"));
		pb_detach->setIconSize(QSize(16,16));
		pb_detach->setToolTip(tr("Open a detached option editor window."));
		buttonLine->addWidget(pb_detach);
		connect(pb_detach, SIGNAL(clicked()), SLOT(detach()));
	}

	
	
	connect(tv_->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), SLOT(selectionChanged(const QModelIndex &)));
	connect(tv_,SIGNAL(activated(const QModelIndex&)),SLOT(tv_edit(const QModelIndex&)));
	connect(tv_,SIGNAL(expanded(const QModelIndex&)), SLOT(updateWidth()));
	connect(tv_,SIGNAL(collapsed(const QModelIndex&)), SLOT(updateWidth()));
	
	
	tv_->setCurrentIndex(tm_->index(0,0, QModelIndex()));

	if (!parent) show();
}

void PsiOptionsEditor::tv_edit( const QModelIndex &idx)
{
	//QModelIndex idx = tv_->currentIndex();
	QString option = tm_->indexToOptionName(idx);
	QVariant value = PsiOptions::instance()->getOption(option);
	if (value.type() == QVariant::Bool) {
		PsiOptions::instance()->setOption(option, QVariant(!value.toBool()));
	} else {
		edit();
	}
}

void PsiOptionsEditor::updateWidth()
{
	if (tv_->columnWidth(0) == tv_colWidth) {
		tv_->resizeColumnToContents(0);
		tv_colWidth = tv_->columnWidth(0);
	}
}

void  PsiOptionsEditor::selectionChanged( const QModelIndex &idx)
{
	QString type = tm_->data(idx.sibling(idx.row(), 1), Qt::DisplayRole).toString();
	QString comment = tm_->data(idx.sibling(idx.row(), 3), Qt::DisplayRole).toString();
	lb_path->setText("<b>"+Qt::escape(tm_->indexToOptionName(idx))+"</b>");
	lb_comment->setText(comment);
	updateWidth();
	QString option = tm_->indexToOptionName(idx);
	QString typ;
	if (o_->isInternalNode(option)) {
		typ = tr("(internal node)");
		pb_edit->setEnabled(false);
	} else {
		typ = tr("Type:") + " <b>" + Qt::escape(type) + "</b>";
		pb_edit->setEnabled(true);
	}
	lb_type->setText("&nbsp;&nbsp;&nbsp;" + typ);

}

void PsiOptionsEditor::add()
{
	QModelIndex idx = tv_->currentIndex();
	QString option = tm_->indexToOptionName(idx);
	if (o_->isInternalNode(option)) {
		option += ".";
	} else {
		option = option.left(option.lastIndexOf(".")+1);
	}
	new OptionEditor(true, option, QVariant());
}

void PsiOptionsEditor::edit()
{
	QModelIndex idx = tv_->currentIndex();
	QString option = tm_->indexToOptionName(idx);
	if (!o_->isInternalNode(option)) {
		new OptionEditor(false, option, PsiOptions::instance()->getOption(option));
	}
}

void PsiOptionsEditor::deleteit()
{
	QModelIndex idx = tv_->currentIndex();
	QString option = tm_->indexToOptionName(idx);
	bool sub = false;
	QString confirm = tr("Really delete options %1?");
	if (o_->isInternalNode(option)) {
		sub = true;
		confirm = tr("Really delete all options starting with %1.?");
	}
	if (QMessageBox::Yes == QMessageBox::warning(this, tr("Psi: Option Editor"),
                   confirm.arg(option), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)) {
		PsiOptions::instance()->removeOption( option, sub);
	}
}

void PsiOptionsEditor::detach()
{
	new PsiOptionsEditor();
}


void PsiOptionsEditor::bringToFront()
{
	::bringToFront(this, true);
}


#include "psioptionseditor.moc"
