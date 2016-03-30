#ifndef CAPTCHADLG_H
#define CAPTCHADLG_H

#include "xmpp_captcha.h"

#include <QDialog>

namespace Ui {
class CaptchaDlg;
}

class PsiAccount;
class XDataWidget;

class CaptchaDlg : public QDialog
{
	Q_OBJECT

public:
	explicit CaptchaDlg(QWidget *parent, const XMPP::CaptchaChallenge &challenge, PsiAccount *pa);
	~CaptchaDlg();

public slots:
	void done(int r);

private slots:
	void captchaFinished();

private:
	Ui::CaptchaDlg *ui;
	XMPP::CaptchaChallenge challenge;
	XDataWidget *dataWidget;
};

#endif // CAPTCHADLG_H
