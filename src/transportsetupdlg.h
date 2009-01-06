#ifndef TRANSPORTSETUPDLG_H
#define TRANSPORTSETUPDLG_H

#include <QtGui>

class PsiAccount;

class TransportSetupDlg : public QDialog
{
	Q_OBJECT
public:
	TransportSetupDlg(PsiAccount *pa, QWidget *parent = 0);
	~TransportSetupDlg();

private:
	class Private;
	Private *d;
};

#endif
