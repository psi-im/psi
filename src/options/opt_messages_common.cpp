#include "opt_messages_common.h"

#include <QWidget>
#include <QDebug>

#include "psioptions.h"


#include "ui_opt_messages_common.h"


class OptMsgCommonUI : public QWidget, public Ui::OptMsgCommon
{
public:
    OptMsgCommonUI() : QWidget() { setupUi(this); }
};

OptionsTabMsgCommon::OptionsTabMsgCommon(QObject *parent)
: OptionsTab(parent, "common", "", tr("Common"), tr("Сommon options for messages section"), "psi/advanced"),
  w_(0),
  psi_(0)
{
}

OptionsTabMsgCommon::~OptionsTabMsgCommon()
{}

QWidget *OptionsTabMsgCommon::widget()
{
    if (w_) {
        return 0;
    }

    w_ = new OptMsgCommonUI();

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);

    connect(d->ck_tabChats, SIGNAL(toggled(bool)), d->cb_tabGrouping, SLOT(setEnabled(bool)));

    d->ck_tabChats->setWhatsThis(
        tr("Makes Psi open chats in a tabbed window."));
    d->ck_showPreviews->setWhatsThis(
        tr("Show under links to some media content preview of the content."
           " It's also possible to play audio and video right in chat."));
    d->ck_showCounter->setWhatsThis(
        tr("Makes Psi show message length counter."
           " Check this if you want to know how long is your message."
           " Can be useful when you're using SMS transport."));
    d->ck_contactsMessageFormatting->setWhatsThis(
        tr("If enabled, Psi will display incoming messages formatted in the style specified by the contact"));

    return w_;
}

void OptionsTabMsgCommon::applyOptions()
{
    if (!w_) {
        return;
    }

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);
    PsiOptions* o = PsiOptions::instance();
    o->setOption("options.ui.message.show-character-count", d->ck_showCounter->isChecked());
    o->setOption("options.html.chat.render", d->ck_contactsMessageFormatting->isChecked());
    o->setOption("options.ui.tabs.show-tab-buttons", d->ck_showTabButtons->isChecked());

    o->setOption("options.ui.tabs.use-tabs", d->ck_tabChats->isChecked());
    QString tabGrouping;
    int idx = d->cb_tabGrouping->currentIndex();
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

    o->setOption("options.ui.tabs.use-tab-shortcuts", d->ck_tabShortcuts->isChecked());
    o->setOption("options.ui.chat.show-previews", d->ck_showPreviews->isChecked());
}

void OptionsTabMsgCommon::restoreOptions()
{
    if (!w_) {
        return;
    }

    OptMsgCommonUI *d = static_cast<OptMsgCommonUI *>(w_);
    PsiOptions* o = PsiOptions::instance();
    d->ck_showCounter->setChecked( o->getOption("options.ui.message.show-character-count").toBool() );
    d->ck_contactsMessageFormatting->setChecked(o->getOption("options.html.chat.render").toBool());
    d->ck_showTabButtons->setChecked( o->getOption("options.ui.tabs.show-tab-buttons").toBool() );
    d->ck_tabChats->setChecked( o->getOption("options.ui.tabs.use-tabs").toBool() );
    d->cb_tabGrouping->setEnabled(o->getOption("options.ui.tabs.use-tabs").toBool());
    QString tabGrouping = o->getOption("options.ui.tabs.grouping").toString();
    bool custom = false;
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
    d->ck_tabShortcuts->setChecked( o->getOption("options.ui.tabs.use-tab-shortcuts").toBool() );
    d->ck_showPreviews->setChecked( o->getOption("options.ui.chat.show-previews").toBool() );
}

void OptionsTabMsgCommon::setData(PsiCon *psi, QWidget *)
{
    psi_ = psi;
}
