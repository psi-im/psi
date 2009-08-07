#ifndef MUCREASONSEDITOR_H
#define MUCREASONSEDITOR_H

#include <QDialog>

#include "ui_mucreasonseditor.h"

class MUCReasonsEditor: public QDialog
{
	Q_OBJECT
public:
	MUCReasonsEditor(QWidget* parent = 0);
	~MUCReasonsEditor();
	QString reason() const { return reason_; }
private:
	Ui::MUCReasonsEditor ui_;
	QString reason_;
private slots:
	void on_btnAdd_clicked();
	void on_btnRemove_clicked();
protected slots:
	void accept();
};

#endif
