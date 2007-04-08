#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include "ui_ui_options.h"
#include <QDialog>

class PsiCon;
struct Options;

class OptionsDlg : public QDialog, public Ui::OptionsUI
{
	Q_OBJECT
public:
	OptionsDlg(PsiCon *, const Options &, QWidget *parent = 0);
	~OptionsDlg();

	void openTab(const QString& id);

signals:
	void applyOptions(const Options &);

private slots:
	void doOk();
	void doApply();

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

#endif
