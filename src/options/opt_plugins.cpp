#include "opt_plugins.h"

#include "common.h"
#include "iconwidget.h"
#include "optionsdlgbase.h"
#include "pluginmanager.h"
#include "psicon.h"
#include "psiiconset.h"
#include "psioptions.h"
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
        setMinimumSize(440, 300);

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

void OptionsTabPlugins::applyOptions() {}

void OptionsTabPlugins::restoreOptions() {}

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
    foreach (const QString &plugin, plugins) {
        QIcon            icon    = pm->icon(plugin);
        bool             enabled = pm->isEnabled(plugin);
        const QString    path    = pm->pathToPlugin(plugin);
        QString          toolTip = tr("Plugin Path:\n%1").arg(path);
        Qt::CheckState   state   = enabled ? Qt::Checked : Qt::Unchecked;
        QTreeWidgetItem *item    = new QTreeWidgetItem(d->tw_Plugins, QTreeWidgetItem::Type);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setText(C_NAME, plugin);
        item->setText(C_VERSION, pm->version(plugin));
        item->setTextAlignment(C_VERSION, Qt::AlignHCenter);
        item->setToolTip(C_NAME, toolTip);
        item->setCheckState(C_NAME, state);
        if (!enabled) {
            icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
        }
        item->setIcon(C_NAME, icon);
        QString   shortName = PluginManager::instance()->shortName(plugin);
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
    QString        name   = item->text(C_NAME);
    QString        option = QString("%1.%2").arg(PluginManager::loadOptionPrefix).arg(pm->shortName(name));
    PsiOptions::instance()->setOption(option, enabled);

    d->tw_Plugins->blockSignals(true); // Block signalls to change item elements
    d->tw_Plugins->itemWidget(item, C_SETTS)->setEnabled(enabled);
    QIcon icon = pm->icon(name);
    if (!enabled) {
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

        infoDialog = new QDialog(d);
        ui_.setupUi(infoDialog);
        QString name = d->tw_Plugins->currentItem()->text(C_NAME);
        infoDialog->setWindowTitle(QString("%1 %2").arg(infoDialog->windowTitle()).arg(name));
        infoDialog->setWindowIcon(QIcon(IconsetFactory::iconPixmap("psi/logo_128")));
        ui_.te_info->setText(PluginManager::instance()->pluginInfo(name));
        infoDialog->setAttribute(Qt::WA_DeleteOnClose);
        infoDialog->show();
    }
}

void OptionsTabPlugins::settingsClicked(int item)
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);
    d->tw_Plugins->setCurrentItem(d->tw_Plugins->topLevelItem(item));

    if (d->tw_Plugins->selectedItems().size() > 0) {
        const QString      pluginName = d->tw_Plugins->currentItem()->text(C_NAME);
        const QString      shortName  = PluginManager::instance()->shortName(pluginName);
        PluginsOptionsDlg *sw         = d->findChild<PluginsOptionsDlg *>(shortName);
        if (!sw) {
            sw = new PluginsOptionsDlg(pluginName, psi, d);
            sw->setObjectName(shortName);
        }
        sw->open();
        sw->adjustSize();
    }
}

#include "opt_plugins.moc"
