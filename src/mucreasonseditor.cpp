#include "mucreasonseditor.h"
#include "common.h"
#include "psioptions.h"


MUCReasonsEditor::MUCReasonsEditor(QWidget* parent)
	: QDialog(parent)
{
	ui_.setupUi(this);
	ui_.lstReasons->addItems(PsiOptions::instance()->getOption("options.muc.reasons").toStringList());
}

MUCReasonsEditor::~MUCReasonsEditor()
{	
}

void MUCReasonsEditor::accept()
{
	QStringList reasons;
	int cnt=ui_.lstReasons->count();
	for (int i=0; i<cnt; ++i)
		reasons.append(ui_.lstReasons->item(i)->text());
	PsiOptions::instance()->setOption("options.muc.reasons", reasons);
	reason_=ui_.txtReason->text();
	QDialog::accept();
}

void MUCReasonsEditor::on_btnAdd_clicked()
{
	reason_=ui_.txtReason->text().trimmed();
	if (reason_.isEmpty())
		return;
	ui_.lstReasons->addItem(reason_);
}

void MUCReasonsEditor::on_btnRemove_clicked()
{
	int idx=ui_.lstReasons->currentRow();
	if (idx>=0) {
		QListWidgetItem *item =ui_.lstReasons->takeItem(idx);
		if (item) delete item;
	}
}

