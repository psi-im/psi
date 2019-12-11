#ifndef PSIOPTIONSEDITOR_H
#define PSIOPTIONSEDITOR_H

#include "optionstreemodel.h"

#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QtCore>

class PsiOptions;
class QSortFilterProxyModel;

class PsiOptionsEditor : public QWidget {
    Q_OBJECT
public:
    PsiOptionsEditor(QWidget *parent = nullptr);
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

protected:
    void keyPressEvent(QKeyEvent *e);

private:
    PsiOptions *           o_;
    QTreeView *            tv_;
    int                    tv_colWidth;
    OptionsTreeModel *     tm_;
    QSortFilterProxyModel *tpm_;
    QCheckBox *            cb_;
    QLabel *               lb_type;
    QLabel *               lb_path;
    QLabel *               lb_comment;
    QLabel *               lb_filter;
    QPushButton *          pb_delete;
    QPushButton *          pb_reset;
    QPushButton *          pb_edit;
    QPushButton *          pb_new;
    QToolButton *          pb_detach;
    QLineEdit *            le_filter;
};

#endif // PSIOPTIONSEDITOR_H
