#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include "ui_ui_options.h"
#include <QDialog>

class PsiCon;

class OptionsDlg : public QDialog, public Ui::OptionsUI
{
	Q_OBJECT
public:
	OptionsDlg(PsiCon *, QWidget *parent = 0);
	~OptionsDlg();

	void openTab(const QString& id);

signals:
	void applyOptions();

private slots:
	void doOk();
	void doApply();

public:
	class Private;
private:
	Private *d;
	friend class Private;

	QPushButton* pb_apply;
};

#endif
