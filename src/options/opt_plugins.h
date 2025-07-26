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
    enum ColumnName { C_NAME = 0, C_DESCRIPTION = 1, C_VERSION = 2, C_ABOUT = 3, C_SETTS = 4 };

public:
    OptionsTabPlugins(QObject *parent);
    ~OptionsTabPlugins();

    void setData(PsiCon *, QWidget *);

    QWidget *widget();
    void     applyOptions();
    void     restoreOptions();
    bool     stretchable() const;

private:
    QWidget             *w = nullptr;
    QPointer<QDialog>    infoDialog;
    Ui::PluginInfoDialog ui_;
    PsiCon              *psi = nullptr;

private slots:
    void listPlugins();
    void showPluginInfo(QTreeWidgetItem *item);
    void savePluginInfoDialogSize();
    void itemChanged(QTreeWidgetItem *item, int column);
    void settingsClicked(QTreeWidgetItem *item);
    void savePluginSettingsDialogSize();

private:
    inline QString formatVendorText(const QString &text, const bool removeEmail);
};

#endif // OPT_PLUGINS_H
