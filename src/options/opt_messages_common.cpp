#include "opt_messages_common.h"

#include "mediadownloadpolicy.h"

#include "psioptions.h"
#include "ui_opt_messages_common.h"

#include <QLocale>
#include <QWidget>

static const QStringList clickActList = { "none", "hide", "close", "detach" };

static QString autoDownloadText(quint64 size)
{
    if (!size)
        return OptionsTabMsgCommon::tr("Never, only when asked");
    // Whole units for the offered sizes, exact ones for a size set by hand.
    const int precision = MediaDownloadPolicy::choices().contains(size) ? 0 : 2;
    return OptionsTabMsgCommon::tr("Up to %1").arg(QLocale().formattedDataSize(qint64(size), precision));
}

class OptMsgCommonUI : public QWidget, public Ui::OptMsgCommon {
public:
    OptMsgCommonUI() : QWidget() { setupUi(this); }
};

OptionsTabMsgCommon::OptionsTabMsgCommon(QObject *parent) :
    OptionsTab(parent, "common", "", tr("Common"), tr("Сommon options for messages section"), "psi/advanced"),
    w_(nullptr), psi_(nullptr)
{
}

OptionsTabMsgCommon::~OptionsTabMsgCommon() { }

QWidget *OptionsTabMsgCommon::widget()
{
    if (w_) {
        return nullptr;
    }

    w_ = new OptMsgCommonUI();

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);

    connect(d->ck_tabChats, &QCheckBox::toggled, d->cb_tabGrouping, &QComboBox::setEnabled);
    connect(d->ck_tabChats, &QCheckBox::toggled, d->cb_tabMdlClick, &QComboBox::setEnabled);
    connect(d->ck_tabChats, &QCheckBox::toggled, d->cb_tabDblClick, &QComboBox::setEnabled);
    connect(d->ck_tabChats, &QCheckBox::toggled, d->ck_showTabButtons, &QCheckBox::setEnabled);
    connect(d->ck_tabChats, &QCheckBox::toggled, d->ck_tabShortcuts, &QCheckBox::setEnabled);

    d->ck_tabChats->setToolTip(tr("Makes Psi open chats in a tabbed window."));
    d->ck_showPreviews->setToolTip(tr("Show under links to some media content preview of the content."
                                      " It's also possible to play audio and video right in chat."));
    for (auto *combo : { d->cb_autoDownloadChats, d->cb_autoDownloadGroupchats }) {
        for (const quint64 size : MediaDownloadPolicy::choices())
            combo->addItem(autoDownloadText(size), qulonglong(size));
        connect(d->ck_showPreviews, &QCheckBox::toggled, combo, &QComboBox::setEnabled);
    }
    connect(d->ck_showPreviews, &QCheckBox::toggled, d->lb_autoDownloadChats, &QLabel::setEnabled);
    connect(d->ck_showPreviews, &QCheckBox::toggled, d->lb_autoDownloadGroupchats, &QLabel::setEnabled);
    d->cb_autoDownloadChats->setToolTip(
        tr("Pictures, voice messages and other media from contacts in your roster, and from your own other"
           " devices, are downloaded and shown right away up to this size. Larger files and files from"
           " anyone else are downloaded only when you ask."));
    d->cb_autoDownloadGroupchats->setToolTip(
        tr("Media in group chats is downloaded and shown right away up to this size, whoever sent it."
           " Larger files are downloaded only when you ask."));
    d->ck_showCounter->setToolTip(tr("Makes Psi show message length counter."
                                     " Check this if you want to know how long is your message."
                                     " Can be useful when you're using SMS transport."));
    d->ck_contactsMessageFormatting->setToolTip(
        tr("If enabled, Psi will display incoming messages formatted in the style specified by the contact by using the deprecated XHTML-IM (XEP-0071)"));
    d->ck_sendHtmlsMessageFormatting->setToolTip(
        tr("If enabled, Psi will send outcoming formatted messages with additional XHTML-IM (XEP-0071). Note: it's deprecated badly supported."));

    d->cb_tabMdlClick->addItems(clickActList);
    d->cb_tabDblClick->addItems(clickActList);

    return w_;
}

void OptionsTabMsgCommon::applyOptions()
{
    if (!w_) {
        return;
    }

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);
    PsiOptions     *o = PsiOptions::instance();
    o->setOption("options.ui.message.show-character-count", d->ck_showCounter->isChecked());
    o->setOption("options.html.chat.render", d->ck_contactsMessageFormatting->isChecked());
    o->setOption("options.html.chat.send", d->ck_sendHtmlsMessageFormatting->isChecked());
    o->setOption("options.media.audio-message", d->ck_audioMessage->isChecked());
    if (d->ck_showTabButtons->isEnabled())
        o->setOption("options.ui.tabs.show-tab-buttons", d->ck_showTabButtons->isChecked());

    o->setOption("options.ui.tabs.use-tabs", d->ck_tabChats->isChecked());
    if (d->cb_tabGrouping->isEnabled()) {
        QString tabGrouping;
        int     idx = d->cb_tabGrouping->currentIndex();
        switch (idx) {
        case 0:
            tabGrouping = "C";
            break;
        case 1:
            tabGrouping = "M";
            break;
        case 2:
            tabGrouping = "C:M";
            break;
        case 3:
            tabGrouping = "CM";
            break;
        case 4:
            tabGrouping = "ACM";
            break;
        }
        if (!tabGrouping.isEmpty()) {
            o->setOption("options.ui.tabs.grouping", tabGrouping);
        } else {
            if (d->cb_tabGrouping->count() == 6) {
                d->cb_tabGrouping->removeItem(5);
            }
        }
    }

    if (d->ck_tabShortcuts->isEnabled())
        o->setOption("options.ui.tabs.use-tab-shortcuts", d->ck_tabShortcuts->isChecked());
    o->setOption("options.ui.chat.show-previews", d->ck_showPreviews->isChecked());
    // The options tree stores these as int, as declared in default.xml
    o->setOption("options.ui.chat.auto-download-size", d->cb_autoDownloadChats->currentData().toInt());
    o->setOption("options.ui.chat.groupchat-auto-download-size", d->cb_autoDownloadGroupchats->currentData().toInt());
    if (d->cb_tabMdlClick->isEnabled())
        o->setOption("options.ui.tabs.mouse-middle-button", d->cb_tabMdlClick->currentText());
    if (d->cb_tabDblClick->isEnabled())
        o->setOption("options.ui.tabs.mouse-doubleclick-action", d->cb_tabDblClick->currentText());
}

void OptionsTabMsgCommon::restoreOptions()
{
    if (!w_) {
        return;
    }

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);
    PsiOptions     *o = PsiOptions::instance();
    d->ck_showCounter->setChecked(o->getOption("options.ui.message.show-character-count").toBool());
    d->ck_contactsMessageFormatting->setChecked(o->getOption("options.html.chat.render", true).toBool());
    d->ck_sendHtmlsMessageFormatting->setChecked(o->getOption("options.html.chat.send", true).toBool());
    d->ck_showTabButtons->setChecked(o->getOption("options.ui.tabs.show-tab-buttons").toBool());
    d->ck_tabChats->setChecked(o->getOption("options.ui.tabs.use-tabs").toBool());
    d->cb_tabGrouping->setEnabled(o->getOption("options.ui.tabs.use-tabs").toBool());
    d->ck_audioMessage->setChecked(o->getOption("options.media.audio-message").toBool());
    QString tabGrouping = o->getOption("options.ui.tabs.grouping").toString();
    bool    custom      = false;
    if (tabGrouping == "C") {
        d->cb_tabGrouping->setCurrentIndex(0);
    } else if (tabGrouping == "M") {
        d->cb_tabGrouping->setCurrentIndex(1);
    } else if (tabGrouping == "C:M") {
        d->cb_tabGrouping->setCurrentIndex(2);
    } else if (tabGrouping == "CM") {
        d->cb_tabGrouping->setCurrentIndex(3);
    } else if (tabGrouping == "ACM") {
        d->cb_tabGrouping->setCurrentIndex(4);
    } else {
        if (d->cb_tabGrouping->count() == 6) {
            d->cb_tabGrouping->setCurrentIndex(5);
        } else {
            d->cb_tabGrouping->setCurrentIndex(-1);
        }
        custom = true;
    }
    if (!custom && d->cb_tabGrouping->count() == 6) {
        d->cb_tabGrouping->removeItem(5);
    }
    d->ck_tabShortcuts->setChecked(o->getOption("options.ui.tabs.use-tab-shortcuts").toBool());
    d->ck_showPreviews->setChecked(o->getOption("options.ui.chat.show-previews").toBool());
    const auto selectSize = [](QComboBox *combo, qulonglong size) {
        int at = combo->findData(size);
        if (at < 0) { // a size set by hand in the options tree
            combo->addItem(autoDownloadText(size), size);
            at = combo->count() - 1;
        }
        combo->setCurrentIndex(at);
    };
    selectSize(d->cb_autoDownloadChats, o->getOption("options.ui.chat.auto-download-size").toULongLong());
    selectSize(d->cb_autoDownloadGroupchats,
               o->getOption("options.ui.chat.groupchat-auto-download-size").toULongLong());
    const bool previews = d->ck_showPreviews->isChecked();
    d->lb_autoDownloadChats->setEnabled(previews);
    d->cb_autoDownloadChats->setEnabled(previews);
    d->lb_autoDownloadGroupchats->setEnabled(previews);
    d->cb_autoDownloadGroupchats->setEnabled(previews);

    QString clickAct = o->getOption("options.ui.tabs.mouse-middle-button").toString();
    if (clickActList.contains(clickAct))
        d->cb_tabMdlClick->setCurrentIndex(clickActList.indexOf(clickAct));
    clickAct = o->getOption("options.ui.tabs.mouse-doubleclick-action").toString();
    if (clickActList.contains(clickAct))
        d->cb_tabDblClick->setCurrentIndex(clickActList.indexOf(clickAct));
}

void OptionsTabMsgCommon::setData(PsiCon *psi, QWidget *) { psi_ = psi; }

bool OptionsTabMsgCommon::stretchable() const { return true; }
