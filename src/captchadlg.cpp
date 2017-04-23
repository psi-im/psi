#include "xmpp_captcha.h"
#include "xmpp_client.h"
#include "xmpp_tasks.h"

#include "captchadlg.h"
#include "ui_captchadlg.h"
#include "xdata_widget.h"
#include "psiaccount.h"

using namespace XMPP;

CaptchaDlg::CaptchaDlg(QWidget *parent, const CaptchaChallenge &challenge, PsiAccount *pa) :
    QDialog(parent),
    ui(new Ui::CaptchaDlg),
    challenge(challenge)
{
	ui->setupUi(this);

	QVBoxLayout *l = new QVBoxLayout;
	dataWidget = new XDataWidget(pa->psi(), this, pa->client(), challenge.arbiter());
	dataWidget->setForm(challenge.form());
	l->addWidget(dataWidget);
	l->addStretch();
	l->addWidget(ui->buttonBox);
	setLayout(l);
}

CaptchaDlg::~CaptchaDlg()
{
	delete ui;
}

void CaptchaDlg::done(int r)
{
	if (!challenge.isValid()) {
		return;
	}

	if (r == QDialog::Accepted) {
		XData::FieldList fl = dataWidget->fields();
		XData resp;
		resp.setType(XData::Data_Form);
		resp.setFields(fl);

		JT_CaptchaSender *t = new JT_CaptchaSender(dataWidget->client()->rootTask());
		connect(t, SIGNAL(finished()), SLOT(captchaFinished()));
		t->set(challenge.arbiter(), resp);
		t->go(true);
	} else {
		QDialog::done(r);
	}
}

void CaptchaDlg::captchaFinished()
{
	JT_CaptchaSender *t = (JT_CaptchaSender *)sender();
	if (t->success()) {
		qDebug("captcha passed");
	} else {
		qDebug("captcha failed");
	}
	QDialog::done(Accepted);
}
