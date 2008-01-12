#ifndef _PSIOPTIONSEDITOR_H_
#define _PSIOPTIONSEDITOR_H_


#include <QtCore>
#include <QTreeView>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include "optionstreemodel.h"

class PsiOptions;

class PsiOptionsEditor : public QWidget
{
	Q_OBJECT
public:
	PsiOptionsEditor(QWidget *parent=0);
	void bringToFront();

private slots:
	void tv_edit(const QModelIndex &idx);
	void selectionChanged(const QModelIndex &idx);
	void updateWidth();
	void add();
	void edit();
	void deleteit();
	void detach();

private:
	PsiOptions *o_;
	QTreeView* tv_;
	int tv_colWidth;
	OptionsTreeModel* tm_;
	QCheckBox* cb_;
	QLabel*	lb_type;
	QLabel* lb_path;
	QLabel* lb_comment;
	QPushButton *pb_delete;
	QPushButton *pb_edit;
	QPushButton *pb_new;
	QToolButton *pb_detach;
};



#endif /* _PSIOPTIONSEDITOR_H_ */
