#ifndef SENDBUTTONMENU_H
#define SENDBUTTONMENU_H

#include <QDialog>
#include <QMenu>
#include <QPointer>

#include "ui_sendbuttontemplateseditor.h"

class SendButtonTemplatesEditor : public QDialog
{
    Q_OBJECT
public:
    SendButtonTemplatesEditor(QWidget* parent = nullptr);

private:
    Ui::SendButtonTemplatesEditor ui_;
    //QString t;
    QAction *addChildTemplAction;
    QAction *addChildSeparatorAction;
    QAction *editAction;
    QAction *removeAction;
    QAction *upAction;
    QAction *downAction;
    void swapItem(int);
    QStringList genTemplatesList(QTreeWidgetItem* item, QString base_path = "");

private slots:
    void addRootTemplate();
    void addChildTemplate();
    void addRootSeparator();
    void addChildSeparator();
    void removeTemplate();
    void editTemplate();
    void upTemplate();
    void downTemplate();
    void selectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void contextMenu(QPoint);

protected slots:
    void accept();
    void reject();

};

class SendButtonTemplatesMenu : public QMenu
{
    Q_OBJECT
public:
    SendButtonTemplatesMenu(QWidget* parent);
    void setParams(bool ps);
    static QStringList getMenuItems(const QString);
    static QString addSlashes(QString);
    static QString stripSlashes(QString);

private:
    bool ps_;
    QAction *pasteSendAct;
    QAction *onlyPaste;
    void updatePsAction();
    QAction *makeAction(const QString);

private slots:
    void update();
    void clickOnlyPaste();
    void optionChanged(const QString& option);

signals:
    void doPasteAndSend();
    void doEditTemplates();
    void doTemplateText(const QString &);

};

#endif
