#include "groupchattopicdlg.h"
#include "ui_groupchattopicdlg.h"

#include "psioptions.h"
#include "groupchatdlg.h"

GroupchatTopicDlg::GroupchatTopicDlg(GCMainDlg *parent) :
	QDialog(parent),
	m_ui(new Ui::GroupchatTopicDlg)
{
	m_ui->setupUi(this);
	QFont f;
	f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());
	m_ui->pte_topic->setFont(f);
	m_ui->pte_topic->setPlainText(parent->topic());
}

GroupchatTopicDlg::~GroupchatTopicDlg()
{
	delete m_ui;
}

QString GroupchatTopicDlg::topic() const
{
	return m_ui->pte_topic->toPlainText();
}

void GroupchatTopicDlg::accept()
{
	emit topicAccepted(m_ui->pte_topic->toPlainText());
	QDialog::accept();
}

void GroupchatTopicDlg::changeEvent(QEvent *e)
{
	QDialog::changeEvent(e);
	switch (e->type()) {
		case QEvent::LanguageChange:
			m_ui->retranslateUi(this);
			break;
		default:
			break;
	}
}
