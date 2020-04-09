#ifndef OPT_PLUGINS_H
#define OPT_PLUGINS_H

#include "advwidget.h"
#include "optionstab.h"
#include "ui_plugininfodialog.h"

#include <QPointer>
#include <QTreeWidgetItem>

class Options;
class QWidget;

class OptionsTabPlugins : public OptionsTab {
    Q_OBJECT
    enum ColumnName { C_NAME = 0, C_VERSION = 1, C_ABOUT = 2, C_SETTS = 3 };

public:
    OptionsTabPlugins(QObject *parent);
    ~OptionsTabPlugins();

    void setData(PsiCon *, QWidget *);

    QWidget *widget();
    void     applyOptions();
    void     restoreOptions();
    bool     stretchable() const;

private:
    QWidget *            w = nullptr;
    QPointer<QDialog>    infoDialog;
    Ui::PluginInfoDialog ui_;
    PsiCon *             psi = nullptr;

private slots:
    void listPlugins();
    void showPluginInfo(int item);
    void itemChanged(QTreeWidgetItem *item, int column);
    void settingsClicked(int item);
};

#endif // OPT_PLUGINS_H
