#include <QPlainTextEdit>

#include "groupchattopicdlg.h"
#include "ui_groupchattopicdlg.h"
#include "psioptions.h"
#include "groupchatdlg.h"
#include "shortcutmanager.h"

GroupchatTopicDlg::GroupchatTopicDlg(GCMainDlg *parent) :
	QDialog(parent),
	m_ui(new Ui::GroupchatTopicDlg)
{
	m_ui->setupUi(this);
	QKeySequence sendKey = ShortcutManager::instance()->shortcut("chat.send");
	if (sendKey == QKeySequence(Qt::Key_Enter) || sendKey == QKeySequence(Qt::Key_Return)) {
		sendKey = QKeySequence(Qt::CTRL + Qt::Key_Return);
	}
	m_ui->buttonBox->button(QDialogButtonBox::Ok)->setShortcut(sendKey);

	auto cw = new QToolButton();
	cw->setIcon(IconsetFactory::icon("psi/add").icon());
	m_ui->twLang->setCornerWidget(cw);
}

GroupchatTopicDlg::~GroupchatTopicDlg()
{
	delete m_ui;
}

QMap<LanguageManager::LangId, QString> GroupchatTopicDlg::subjectMap() const
{
	QMap<LanguageManager::LangId, QString> ret;
	for (int i = 0; i < m_ui->twLang->count(); i++) {
		QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(m_ui->twLang->widget(i));
		LanguageManager::LangId id = edit->property("lngId").value<LanguageManager::LangId>();
		QString text = edit->toPlainText();
		ret.insert(id, text);
	}
	return ret;
}

void GroupchatTopicDlg::setSubjectMap(const QMap<LanguageManager::LangId, QString> &topics)
{
	QFont f;
	f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());

	for (auto it = topics.constBegin(); it != topics.constEnd(); ++it) {
		QPlainTextEdit *edit = new QPlainTextEdit(it.value());
		edit->setFont(f);
		edit->setProperty("langId", QVariant::fromValue<LanguageManager::LangId>(it.key()));
		m_ui->twLang->addTab(edit, LanguageManager::languageName(it.key()));
	}
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
