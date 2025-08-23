#include "captchadlg.h"

#include "iris/xmpp_captcha.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_tasks.h"
#include "psiaccount.h"
#include "ui_captchadlg.h"
#include "xdata_widget.h"

using namespace XMPP;

CaptchaDlg::CaptchaDlg(QWidget *parent, XMPP::Jid from, QString msg, const CaptchaChallenge &challenge, PsiAccount *pa) :
    QDialog(parent), ui(new Ui::CaptchaDlg), challenge(challenge)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);

    ui->lb_ident->setAccount(pa);
    ui->lb_from->setText(from.full());
    ui->te_message->setText(msg);
    dataWidget     = new XDataWidget(pa->psi(), this, pa->client(), challenge.arbiter());
    dataWidget->setForm(challenge.form());
    ui->layoutForm->addWidget(dataWidget);
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
