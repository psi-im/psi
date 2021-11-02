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

    QString pluginShortName;

public:
    OptionsTabPlugin(const QString &pluginShortName, QObject *parent) :
        OptionsTab(parent, "general", "", tr("General"), tr("General plugin options"), "psi/options"),
        pluginShortName(pluginShortName)
    {
    }
    QWidget *widget() { return PluginManager::instance()->optionsWidget(pluginShortName); }
    void     applyOptions() { PluginManager::instance()->applyOptions(pluginShortName); }
    void     restoreOptions() { PluginManager::instance()->restoreOptions(pluginShortName); }
};

class PluginsOptionsDlg : public OptionsDlgBase {
    Q_OBJECT

public:
    PluginsOptionsDlg(const QString &shortPluginName, PsiCon *psi, QWidget *parent = nullptr) :
        OptionsDlgBase(psi, parent)
    {
        setWindowTitle(QString("%1: %2").arg(PluginManager::instance()->pluginName(shortPluginName), windowTitle()));
        QIcon icon = PluginManager::instance()->icon(shortPluginName);
        if (icon.isNull()) {
            icon = IconsetFactory::iconPtr("psi/options")->icon();
        }
        setWindowIcon(icon);
        setTabs({ new OptionsTabPlugin(shortPluginName, this) });

        psi->dialogRegister(this);

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

    QStringList plugins    = pm->availablePlugins();
    const QSize buttonSize = QSize(21, 21);
    for (const QString &shortName : plugins) {
        QIcon         icon       = pm->icon(shortName);
        bool          enabled    = pm->isEnabled(shortName);
        const QString path       = pm->pathToPlugin(shortName);
        auto          pluginName = pm->pluginName(shortName);

        auto    vendors = formatVendorText(pm->vendor(shortName), true);
        QString toolTip = QString("<b>%1 %2</b><br/>%3<br/><br/><b>%4:</b><br/>%5<br/><br/><b>%6:</b><br/>%7")
                              .arg(pluginName, pm->version(shortName), TextUtil::plain2rich(pm->description(shortName)),
                                   tr("Authors"), vendors, tr("Plugin Path"), path);

        Qt::CheckState   state               = enabled ? Qt::Checked : Qt::Unchecked;
        QTreeWidgetItem *item                = new QTreeWidgetItem(d->tw_Plugins, QTreeWidgetItem::Type);
        auto             truncatedPluginName = QString(pluginName).replace(" Plugin", "");
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(C_NAME, Qt::UserRole, shortName);
        item->setText(C_NAME, truncatedPluginName);
        item->setText(C_VERSION, pm->version(shortName));
        item->setTextAlignment(C_VERSION, Qt::AlignHCenter);
        item->setToolTip(C_NAME, toolTip);
        item->setCheckState(C_NAME, state);
        if (!enabled && !icon.isNull()) {
            icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
        }
        item->setIcon(C_NAME, icon);

        QToolButton *aboutbutton = new QToolButton(d->tw_Plugins);
        aboutbutton->setIcon(QIcon(IconsetFactory::iconPtr(QLatin1String("psi/info"))->icon()));
        aboutbutton->resize(buttonSize);
        aboutbutton->setObjectName("ab_" + shortName);
        aboutbutton->setToolTip(tr("Show information about plugin"));
        aboutbutton->setEnabled(enabled);
        d->tw_Plugins->setItemWidget(item, C_ABOUT, aboutbutton);
        connect(aboutbutton, &QToolButton::clicked, this, [item, this](bool) { showPluginInfo(item); });

        QToolButton *settsbutton = new QToolButton(d->tw_Plugins);
        settsbutton->setIcon(QIcon(IconsetFactory::iconPtr(QLatin1String("psi/options"))->icon()));
        settsbutton->resize(buttonSize);
        settsbutton->setObjectName("sb_" + shortName);
        settsbutton->setToolTip(tr("Open plugin settings dialog"));
        settsbutton->setEnabled(enabled);
        d->tw_Plugins->setItemWidget(item, C_SETTS, settsbutton);
        connect(settsbutton, &QToolButton::clicked, this, [item, this](bool) { settingsClicked(item); });
    }
    if (d->tw_Plugins->topLevelItemCount() > 0) {
        d->tw_Plugins->sortItems(C_NAME, Qt::AscendingOrder);
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
    QString        option = QString("%1.%2").arg(PluginManager::loadOptionPrefix, name);
    PsiOptions::instance()->setOption(option, enabled);

    d->tw_Plugins->blockSignals(true); // Block signalls to change item elements
    d->tw_Plugins->itemWidget(item, C_SETTS)->setEnabled(enabled);
    d->tw_Plugins->itemWidget(item, C_ABOUT)->setEnabled(enabled);
    QIcon icon = pm->icon(name);
    if (!enabled && !icon.isNull()) {
        icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
    }
    item->setIcon(C_NAME, icon);
    d->tw_Plugins->blockSignals(false); // Release signals blocking
}

void OptionsTabPlugins::showPluginInfo(QTreeWidgetItem *item)
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);

    if (infoDialog)
        delete (infoDialog);

    const QSize dialogSize
        = PsiOptions::instance()->getOption("options.ui.save.plugin-info-dialog-size", QSize(600, 400)).toSize();

    infoDialog = new QDialog(d);
    ui_.setupUi(infoDialog);
    infoDialog->setWindowTitle(tr("About plugin"));
    infoDialog->setWindowIcon(QIcon(IconsetFactory::iconPtr("psi/logo_128")->icon()));
    const QString &shortName = item->data(C_NAME, Qt::UserRole).toString();
    ui_.tb_info->setText(PluginManager::instance()->pluginInfo(shortName));
    auto vendor   = formatVendorText(PluginManager::instance()->vendor(shortName), false);
    int  iconSize = ui_.lbl_icon->fontInfo().pixelSize() * 2.5;
    ui_.lbl_icon->setPixmap(PluginManager::instance()->icon(shortName).pixmap(iconSize, QIcon::Normal, QIcon::On));

    auto name = PluginManager::instance()->pluginName(shortName);
    ui_.lbl_meta->setText(QString("<b>%1 %2</b><br/><b>%3:</b> %4")
                              .arg(name, PluginManager::instance()->version(shortName), tr("Authors"), vendor));
    ui_.lbl_file->setText(
        QString("<b>%1:</b> %2")
            .arg(tr("Plugin Path"), TextUtil::escape(PluginManager::instance()->pathToPlugin(shortName))));
    infoDialog->resize(dialogSize);
    infoDialog->show();

    connect(infoDialog, &QDialog::finished, this, &OptionsTabPlugins::savePluginInfoDialogSize);
}

void OptionsTabPlugins::savePluginInfoDialogSize()
{
    QDialog *dlg = dynamic_cast<QDialog *>(sender());
    if (!dlg)
        return;

    PsiOptions::instance()->setOption("options.ui.save.plugin-info-dialog-size", dlg->size());
    dlg->deleteLater();
}

void OptionsTabPlugins::settingsClicked(QTreeWidgetItem *item)
{
    if (!w)
        return;

    OptPluginsUI *d = static_cast<OptPluginsUI *>(w);
    const QSize   dialogSize
        = PsiOptions::instance()->getOption("options.ui.save.plugin-settings-dialog-size", QSize(600, 400)).toSize();
    const QString &    shortName = item->data(C_NAME, Qt::UserRole).toString();
    PluginsOptionsDlg *sw        = d->findChild<PluginsOptionsDlg *>(shortName);
    if (!sw) {
        sw = new PluginsOptionsDlg(shortName, psi, d);
        sw->setObjectName(shortName);
    }
    sw->resize(dialogSize);
    sw->open();

    connect(sw, &QDialog::finished, this, &OptionsTabPlugins::savePluginSettingsDialogSize);
}

void OptionsTabPlugins::savePluginSettingsDialogSize()
{
    QDialog *dlg = dynamic_cast<QDialog *>(sender());
    if (!dlg)
        return;

    PsiOptions::instance()->setOption("options.ui.save.plugin-settings-dialog-size", dlg->size());
    dlg->deleteLater();
}

QString OptionsTabPlugins::formatVendorText(const QString &text, const bool removeEmail)
{
    if (!text.contains("@") || !text.contains("<") || !text.contains(">"))
        return TextUtil::escape(text);

    QStringList authors = text.split(", ");
    for (QString &author : authors) {
        if (!author.contains("@") || !author.contains("<") || !author.contains(">"))
            continue;

        QStringList words = author.split(" ");
        if (words.size() < 2)
            continue;

        QString email = words.last();
        if (email.contains("@")) {
            words.removeLast();
            if (!removeEmail) {
                email.remove("<");
                email.remove(">");
                author = TextUtil::escape(words.join(" "));
                author = QString("<a href=\"mailto:%2\">%1</a>").arg(author, TextUtil::escape(email));
            }
        }
    }
    return authors.join(", ");
}

#include "opt_plugins.moc"
