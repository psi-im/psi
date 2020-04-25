#include "opt_application.h"

#include "applicationinfo.h"
#include "common.h"
#include "iconwidget.h"
#include "proxy.h"
#include "psioptions.h"
#include "translationmanager.h"
#include "ui_opt_application.h"
#include "varlist.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
static const QString regString = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
#elif defined(HAVE_FREEDESKTOP)
#define xstr(a) str(a)
#define str(a) #a
static const QString psiAutoStart("/autostart/" xstr(APP_BIN_NAME) ".desktop");
#endif

class OptApplicationUI : public QWidget, public Ui::OptApplication {
public:
    OptApplicationUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabApplication
//----------------------------------------------------------------------------

OptionsTabApplication::OptionsTabApplication(QObject *parent) :
    OptionsTab(parent, "application", "", tr("Application"), tr("General application options"), "psi/logo_32")
{
#ifdef HAVE_FREEDESKTOP
    configPath_ = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
#endif
}

OptionsTabApplication::~OptionsTabApplication() { }

QWidget *OptionsTabApplication::widget()
{
    if (w)
        return nullptr;

    w                   = new OptApplicationUI();
    OptApplicationUI *d = static_cast<OptApplicationUI *>(w);

    // docklet
    d->ck_docklet->setToolTip(tr("Makes Psi use a docklet icon, also known as system tray icon."));
    d->ck_dockDCstyle->setToolTip(tr("Normally, single-clicking on the Psi docklet icon brings the main window to"
                                     " the foreground.  Check this option if you would rather use a double-click."));
    d->ck_dockHideMW->setToolTip(tr("Starts Psi with only the docklet icon visible."));
    d->ck_dockToolMW->setToolTip(tr("Prevents Psi from taking up a slot on the taskbar and makes the main "
                                    "window use a small titlebar."));

#ifdef Q_OS_MAC
    d->gb_docklet->hide();
    d->ck_auto_load->hide();
#endif

    if (!haveAutoUpdater_) {
        d->ck_autoUpdate->hide();
    }

    // Proxy

    ProxyChooser *pc = ProxyManager::instance()->createProxyChooser(w);
    d->gb_proxy->layout()->addWidget(ProxyManager::instance()->proxyForObject()->getComboBox(pc, w));
    d->gb_proxy->layout()->addWidget(pc);

    connect(d->le_dtPort, &QLineEdit::textChanged, this, [this]() {
        if (!w)
            return;

        OptApplicationUI *d = static_cast<OptApplicationUI *>(w);

        if (d->le_dtPort->text().isEmpty()) {
            d->label->clear();
            return;
        }

        int port = d->le_dtPort->text().toInt();
        if (port < 0 || port > 65532) {
            d->label->clear();
            return;
        }

        if (port == 0) {
            d->label->setText(tr("(TCP: Disabled, UDP: Auto)"));
        } else {
            d->label->setText(tr("(TCP: %1, UDP: %1-%2)").arg(port).arg(port + 3));
        }
    });
    connect(d->ck_docklet, &QCheckBox::stateChanged, this, &OptionsTabApplication::doEnableQuitOnClose);
    connect(d->ck_auto_load, &QCheckBox::toggled, this, [this]() { autostartOptChanged_ = true; });

    return w;
}

void OptionsTabApplication::setHaveAutoUpdater(bool b) { haveAutoUpdater_ = b; }

void OptionsTabApplication::applyOptions()
{
    if (!w)
        return;

    OptApplicationUI *d = static_cast<OptApplicationUI *>(w);

    PsiOptions::instance()->setOption("options.ui.contactlist.quit-on-close", d->ck_quitOnClose->isChecked());
    if (!ApplicationInfo::isPortable()) {
        PsiOptions::instance()->setOption("options.keychain.enabled", d->ck_useKeychain->isChecked());
    }

    // Auto-update
    PsiOptions::instance()->setOption("options.auto-update.check-on-startup", d->ck_autoUpdate->isChecked());

    // docklet
    PsiOptions::instance()->setOption("options.ui.systemtray.enable", d->ck_docklet->isChecked());
    PsiOptions::instance()->setOption("options.ui.systemtray.use-double-click", d->ck_dockDCstyle->isChecked());
    PsiOptions::instance()->setOption("options.contactlist.hide-on-start", d->ck_dockHideMW->isChecked());
    PsiOptions::instance()->setOption("options.contactlist.use-toolwindow", d->ck_dockToolMW->isChecked());

    // data transfer
    PsiOptions::instance()->setOption("options.p2p.bytestreams.listen-port", d->le_dtPort->text().toInt());
    PsiOptions::instance()->setOption("options.p2p.bytestreams.external-address", d->le_dtExternal->text().trimmed());

    // Proxy
    ProxyManager::instance()->proxyForObject()->save();

    // Language
    QString curLang = TranslationManager::instance()->currentLanguage();
    QString lang    = d->cb_lang->currentText();
    QString itemData;
    for (VarListItem it : TranslationManager::instance()->availableTranslations()) {
        if (it.data() == lang) {
            itemData = it.key();
            break;
        }
    }
    if (curLang != itemData && !itemData.isEmpty()) {
        TranslationManager::instance()->loadTranslation(itemData);
        QMessageBox::information(nullptr, tr("Information"),
                                 tr("Some of the options you changed will only have full effect upon restart."));
    }
    QSettings s(ApplicationInfo::homeDir(ApplicationInfo::ConfigLocation) + "/psirc", QSettings::IniFormat);
    s.setValue("last_lang", itemData);

    // Auto-load
    if (autostartOptChanged_) {
#ifdef Q_OS_WIN
        QSettings set(regString, QSettings::NativeFormat);
        if (d->ck_auto_load->isChecked()) {
            set.setValue(ApplicationInfo::name(), QDir::toNativeSeparators(qApp->applicationFilePath()));
        } else {
            set.remove(ApplicationInfo::name());
        }
#elif defined(HAVE_FREEDESKTOP)
        QDir home(configPath_);
        home.mkpath("autostart");
        // Create APP_BIN_NAME.desktop file if not exists
        QFile         f(home.absolutePath() + psiAutoStart);
        const QString fContents = "[Desktop Entry]\nVersion=1.1\nType=Application\n"
            + QString("Name=%1\n").arg(ApplicationInfo::name()) + QString("Icon=%1\n").arg(xstr(APP_BIN_NAME))
            + QString("Exec=%1\n").arg(qApp->applicationFilePath())
            + QString("Hidden=%1\n").arg(d->ck_auto_load->isChecked() ? "false" : "true");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(fContents.toUtf8());
        }
#endif
    }
}

void OptionsTabApplication::restoreOptions()
{
    if (!w)
        return;

    OptApplicationUI *d = static_cast<OptApplicationUI *>(w);

    d->ck_autoUpdate->setChecked(PsiOptions::instance()->getOption("options.auto-update.check-on-startup").toBool());
    d->ck_quitOnClose->setChecked(PsiOptions::instance()->getOption("options.ui.contactlist.quit-on-close").toBool());
    if (!ApplicationInfo::isPortable()) {
        d->ck_useKeychain->setChecked(PsiOptions::instance()->getOption("options.keychain.enabled").toBool());
    }

    // docklet
    d->ck_docklet->setChecked(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool());
    d->ck_dockDCstyle->setChecked(PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool());
    d->ck_dockHideMW->setChecked(PsiOptions::instance()->getOption("options.contactlist.hide-on-start").toBool());
    d->ck_dockToolMW->setChecked(PsiOptions::instance()->getOption("options.contactlist.use-toolwindow").toBool());
    doEnableQuitOnClose(d->ck_docklet->isChecked() ? 1 : 0);

    // data transfer
    d->le_dtPort->setText(
        QString::number(PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt()));
    d->le_dtExternal->setText(PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString());

    // Language
    VarList     vList = TranslationManager::instance()->availableTranslations();
    QStringList lang  = vList.varsToStringList();
    d->cb_lang->addItem(tr("Default"));
    for (QString item : lang) {
        d->cb_lang->addItem(vList.get(item));
    }
    QString   curLang = TranslationManager::instance()->currentLanguage();
    QSettings s(ApplicationInfo::homeDir(ApplicationInfo::ConfigLocation) + "/psirc", QSettings::IniFormat);
    QString   curL = s.value("last_lang", "").toString();
    if (curL.isEmpty())
        d->cb_lang->setCurrentIndex(0);
    else if (!curLang.isEmpty() && lang.contains(curLang))
        d->cb_lang->setCurrentIndex(d->cb_lang->findText(vList.get(curLang)));

    // Auto-load
    autostartOptChanged_ = false;
#ifdef Q_OS_WIN
    QSettings     set(regString, QSettings::NativeFormat);
    const QString path = set.value(ApplicationInfo::name()).toString();
    d->ck_auto_load->blockSignals(true);
    d->ck_auto_load->setChecked((path == QDir::toNativeSeparators(qApp->applicationFilePath())));
    d->ck_auto_load->blockSignals(false);
#elif defined(HAVE_FREEDESKTOP)
    QFile desktop(configPath_ + psiAutoStart);
    d->ck_auto_load->blockSignals(true);
    d->ck_auto_load->setChecked(
        desktop.open(QIODevice::ReadOnly)
        && QString(desktop.readAll())
               .contains(QRegularExpression("\\bhidden\\s*=\\s*false", QRegularExpression::CaseInsensitiveOption)));
    d->ck_auto_load->blockSignals(false);
#endif
#ifdef HAVE_KEYCHAIN
    d->ck_useKeychain->setVisible(!ApplicationInfo::isPortable());
#else
    d->ck_useKeychain->setVisible(false);
#endif
}

void OptionsTabApplication::doEnableQuitOnClose(int state)
{
    if (!w)
        return;

    OptApplicationUI *d = static_cast<OptApplicationUI *>(w);
    d->ck_quitOnClose->setEnabled(state > 0);
    d->ck_dockToolMW->setEnabled(state > 0);
    d->ck_dockDCstyle->setEnabled(state > 0);
    d->ck_dockHideMW->setEnabled(state > 0);
}
