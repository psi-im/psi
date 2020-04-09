#include "captchadlg.h"

#include "psiaccount.h"
#include "ui_captchadlg.h"
#include "xdata_widget.h"
#include "xmpp_captcha.h"
#include "xmpp_client.h"
#include "xmpp_tasks.h"

using namespace XMPP;

CaptchaDlg::CaptchaDlg(QWidget *parent, const CaptchaChallenge &challenge, PsiAccount *pa) :
    QDialog(parent), ui(new Ui::CaptchaDlg), challenge(challenge)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);

    QVBoxLayout *l = new QVBoxLayout;
    dataWidget     = new XDataWidget(pa->psi(), this, pa->client(), challenge.arbiter());
    dataWidget->setForm(challenge.form());
    l->addWidget(dataWidget);
    l->addStretch();
    l->addWidget(ui->buttonBox);
    setLayout(l);
}

CaptchaDlg::~CaptchaDlg() { delete ui; }

void CaptchaDlg::done(int r)
{
    if (challenge.isValid() && r == QDialog::Accepted) {
        XData::FieldList fl = dataWidget->fields();
        XData            resp;
        resp.setType(XData::Data_Form);
        resp.setFields(fl);

        JT_CaptchaSender *t = new JT_CaptchaSender(dataWidget->client()->rootTask());
        connect(t, &JT_CaptchaSender::finished, this, [t]() {
            if (t->success()) {
                qDebug("captcha passed");
            } else {
                qDebug("captcha failed");
            }
        });
        t->set(challenge.arbiter(), resp);
        t->go(true);
    }

    QDialog::done(r);
}
