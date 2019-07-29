#ifndef _PSIOPTIONSEDITOR_H_
#define _PSIOPTIONSEDITOR_H_

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QtCore>

#include "optionstreemodel.h"

class PsiOptions;
class QSortFilterProxyModel;

class PsiOptionsEditor : public QWidget
{
    Q_OBJECT
public:
    PsiOptionsEditor(QWidget *parent=nullptr);
    void bringToFront();

private slots:
    void tv_edit(const QModelIndex &idx);
    void selectionChanged(const QModelIndex &idx_f);
    void updateWidth();
    void add();
    void edit();
    void deleteit();
    void resetit();
    void detach();

private:
    PsiOptions *o_;
    QTreeView* tv_;
    int tv_colWidth;
    OptionsTreeModel* tm_;
    QSortFilterProxyModel *tpm_;
    QCheckBox* cb_;
    QLabel*    lb_type;
    QLabel* lb_path;
    QLabel* lb_comment;
    QPushButton *pb_delete;
    QPushButton *pb_reset;
    QPushButton *pb_edit;
    QPushButton *pb_new;
    QToolButton *pb_detach;
};

#endif /* _PSIOPTIONSEDITOR_H_ */
