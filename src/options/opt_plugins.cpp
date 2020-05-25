#include "opt_plugins.h"

#include "common.h"
#include "iconwidget.h"
#include "optionsdlgbase.h"
#include "pluginmanager.h"
#include "psicon.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "textutil.h"
#include "ui_opt_plugins.h"

#include <QHeaderView>
#include <QSignalMapper>
#include <QToolButton>

class OptPluginsUI : public QWidget, public Ui::OptPlugins {
public:
    OptPluginsUI() : QWidget() { setupUi(this); }
};

class OptionsTabPlugin : public OptionsTab {
    Q_OBJECT

    QString pluginName;

public:
    OptionsTabPlugin(const QString &pluginName, QObject *parent) :
        OptionsTab(parent, "general", "", tr("General"), tr("General plugin options"), "psi/options"),
        pluginName(pluginName)
    {
    }
    QWidget *widget() { return PluginManager::instance()->optionsWidget(pluginName); }
    void     applyOptions() { PluginManager::instance()->applyOptions(pluginName); }
    void     restoreOptions() { PluginManager::instance()->restoreOptions(pluginName); }
};

class PluginsOptionsDlg : public OptionsDlgBase {
    Q_OBJECT

public:
    PluginsOptionsDlg(const QString &pluginName, PsiCon *psi, QWidget *parent = nullptr) : OptionsDlgBase(psi, parent)
    {
        setWindowTitle(QString("%1: %2").arg(pluginName).arg(windowTitle()));
        QIcon icon = PluginManager::instance()->icon(pluginName);
        if (icon.isNull()) {
            icon = IconsetFactory::iconPtr("psi/options")->icon();
        }
        setWindowIcon(icon);
        setTabs(QList<OptionsTab *>() << new OptionsTabPlugin(pluginName, this));

        psi->dialogRegister(this);
        setMinimumSize(600, 300);

        openTab("general");
    }
};

//----------------------------------------------------------------------------
// OptionsTabPlugins
//----------------------------------------------------------------------------

OptionsTabPlugins::OptionsTabPlugins(QObject *parent) :
    OptionsTab(parent, "plugins", "", tr("Plugins"), tr("Options for Psi plugins"), "psi/plugins")
{
}

OptionsTabPlugins::~OptionsTabPlugins()
{
    if (infoDialog)
        delete (infoDialog);
}

void OptionsTabPlugins::setData(PsiCon *psi, QWidget *) { this->psi = psi; }

QWidget *OptionsTabPlugins::widget()
{
    if (w)
        return nullptr;

    w               = new OptPluginsUI();
    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);

    listPlugins();
    connect(d->tw_Plugins, &QTreeWidget::itemChanged, this, &OptionsTabPlugins::itemChanged);

    return w;
}

void OptionsTabPlugins::applyOptions() { }

void OptionsTabPlugins::restoreOptions() { }

bool OptionsTabPlugins::stretchable() const { return true; }

void OptionsTabPlugins::listPlugins()
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);

    d->tw_Plugins->clear();

    PluginManager *pm = PluginManager::instance();

    QStringList plugins = pm->availablePlugins();
    plugins.sort();
    const QSize buttonSize = QSize(21, 21);
    for (const QString &plugin : plugins) {
        QIcon         icon    = pm->icon(plugin);
        bool          enabled = pm->isEnabled(plugin);
        const QString path    = pm->pathToPlugin(plugin);
        QString       toolTip = QString("<b>%1 %2</b><br/><b>%3: </b>%4<br/><br/>%5<br/><br/><b>%6:</b><br/>%7")
                              .arg(plugin, pm->version(plugin), tr("Authors"), pm->vendor(plugin),
                                   pm->pluginInfo(plugin), tr("Plugin Path"), path);

        Qt::CheckState   state               = enabled ? Qt::Checked : Qt::Unchecked;
        QTreeWidgetItem *item                = new QTreeWidgetItem(d->tw_Plugins, QTreeWidgetItem::Type);
        auto             truncatedPluginName = QString(plugin).replace(" Plugin", "");
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(C_NAME, Qt::UserRole, plugin);
        item->setText(C_NAME, truncatedPluginName);
        item->setText(C_VERSION, pm->version(plugin));
        item->setTextAlignment(C_VERSION, Qt::AlignHCenter);
        item->setToolTip(C_NAME, toolTip);
        item->setCheckState(C_NAME, state);
        if (!enabled && !icon.isNull()) {
            icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
        }
        item->setIcon(C_NAME, icon);
        QString   shortName = pm->shortName(plugin);
        const int index     = d->tw_Plugins->indexOfTopLevelItem(item);

        QToolButton *aboutbutton = new QToolButton(d->tw_Plugins);
        aboutbutton->setIcon(QIcon(IconsetFactory::iconPixmap("psi/info")));
        aboutbutton->resize(buttonSize);
        aboutbutton->setObjectName("ab_" + shortName);
        aboutbutton->setToolTip(tr("Show information about plugin"));
        d->tw_Plugins->setItemWidget(item, C_ABOUT, aboutbutton);
        connect(aboutbutton, &QToolButton::clicked, this, [index, this](bool) { showPluginInfo(index); });

        QToolButton *settsbutton = new QToolButton(d->tw_Plugins);
        settsbutton->setIcon(QIcon(IconsetFactory::iconPixmap("psi/options")));
        settsbutton->resize(buttonSize);
        settsbutton->setObjectName("sb_" + shortName);
        settsbutton->setToolTip(tr("Open plugin settings dialog"));
        settsbutton->setEnabled(enabled);
        d->tw_Plugins->setItemWidget(item, C_SETTS, settsbutton);
        connect(settsbutton, &QToolButton::clicked, this, [index, this](bool) { settingsClicked(index); });
    }
    if (d->tw_Plugins->topLevelItemCount() > 0) {
        d->tw_Plugins->header()->setSectionResizeMode(C_NAME, QHeaderView::Stretch);
        d->tw_Plugins->resizeColumnToContents(C_VERSION);
        d->tw_Plugins->resizeColumnToContents(C_ABOUT);
        d->tw_Plugins->resizeColumnToContents(C_SETTS);
    }
}

void OptionsTabPlugins::itemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!w)
        return;
    OptPluginsUI *d       = static_cast<OptPluginsUI *>(w);
    bool          enabled = item->checkState(C_NAME) == Qt::Checked;
    d->tw_Plugins->setCurrentItem(item);

    PluginManager *pm     = PluginManager::instance();
    QString        name   = item->data(C_NAME, Qt::UserRole).toString();
    QString        option = QString("%1.%2").arg(PluginManager::loadOptionPrefix).arg(pm->shortName(name));
    PsiOptions::instance()->setOption(option, enabled);

    d->tw_Plugins->blockSignals(true); // Block signalls to change item elements
    d->tw_Plugins->itemWidget(item, C_SETTS)->setEnabled(enabled);
    QIcon icon = pm->icon(name);
    if (!enabled && !icon.isNull()) {
        icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
    }
    item->setIcon(C_NAME, icon);
    d->tw_Plugins->blockSignals(false); // Release signals blocking
}

void OptionsTabPlugins::showPluginInfo(int item)
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);
    d->tw_Plugins->setCurrentItem(d->tw_Plugins->topLevelItem(item));

    if (d->tw_Plugins->selectedItems().size() > 0) {
        if (infoDialog)
            delete (infoDialog);

        const QSize dialogSize
            = PsiOptions::instance()->getOption("options.ui.save.plugin-info-dialog-size", QSize(600, 400)).toSize();

        infoDialog = new QDialog(d);
        ui_.setupUi(infoDialog);
        infoDialog->setWindowTitle(tr("About plugin"));
        infoDialog->setWindowIcon(QIcon(IconsetFactory::iconPixmap("psi/logo_128")));
        const QString &name = d->tw_Plugins->currentItem()->data(C_NAME, Qt::UserRole).toString();
        ui_.tb_info->setText(PluginManager::instance()->pluginInfo(name));

        auto vendors = PluginManager::instance()->vendor(name).split(',');
        for (auto &v : vendors) {
            v = TextUtil::linkify(TextUtil::escape(v.trimmed()));
        }
        QString vendor = vendors.mid(0, vendors.size() - 1).join(", ");
        vendor         = vendor.isEmpty() ? vendors.last() : tr("%1 and %2").arg(vendor, vendors.last());

        int iconSize = ui_.lbl_icon->fontInfo().pixelSize() * 1.2;
        ui_.lbl_icon->setPixmap(PluginManager::instance()->icon(name).pixmap(iconSize, QIcon::Normal, QIcon::On));
        ui_.lbl_meta->setText(tr("<b>%1</b> %2 by %3").arg(name, PluginManager::instance()->version(name), vendor));
        ui_.lbl_file->setText(
            QString("<b>%1:</b> %2")
                .arg(tr("Plugin Path"), TextUtil::escape(PluginManager::instance()->pathToPlugin(name))));
        infoDialog->resize(dialogSize);
        infoDialog->show();

        connect(infoDialog, &QDialog::finished, this, &OptionsTabPlugins::savePluginInfoDialogSize);
    }
}

void OptionsTabPlugins::savePluginInfoDialogSize()
{
    QDialog *dlg = dynamic_cast<QDialog *>(sender());
    if (!dlg)
        return;

    PsiOptions::instance()->setOption("options.ui.save.plugin-info-dialog-size", dlg->size());
    dlg->deleteLater();
}

void OptionsTabPlugins::settingsClicked(int item)
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);
    d->tw_Plugins->setCurrentItem(d->tw_Plugins->topLevelItem(item));

    if (d->tw_Plugins->selectedItems().size() > 0) {
        const QSize dialogSize = PsiOptions::instance()
                                     ->getOption("options.ui.save.plugin-settings-dialog-size", QSize(600, 400))
                                     .toSize();
        const QString &    pluginName = d->tw_Plugins->currentItem()->data(C_NAME, Qt::UserRole).toString();
        const QString &    shortName  = PluginManager::instance()->shortName(pluginName);
        PluginsOptionsDlg *sw         = d->findChild<PluginsOptionsDlg *>(shortName);
        if (!sw) {
            sw = new PluginsOptionsDlg(pluginName, psi, d);
            sw->setObjectName(shortName);
        }
        sw->resize(dialogSize);
        sw->open();

        connect(sw, &QDialog::finished, this, &OptionsTabPlugins::savePluginSettingsDialogSize);
    }
}

void OptionsTabPlugins::savePluginSettingsDialogSize()
{
    QDialog *dlg = dynamic_cast<QDialog *>(sender());
    if (!dlg)
        return;

    PsiOptions::instance()->setOption("options.ui.save.plugin-settings-dialog-size", dlg->size());
    dlg->deleteLater();
}

#include "opt_plugins.moc"
