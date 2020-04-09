#include "opt_advanced.h"

#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "ui_opt_advanced.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>

class OptAdvancedUI : public QWidget, public Ui::OptAdvanced {
public:
    OptAdvancedUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabAdvanced
//----------------------------------------------------------------------------

OptionsTabAdvanced::OptionsTabAdvanced(QObject *parent) :
    OptionsTab(parent, "misc", "", tr("Misc."), tr("Extra uncategorized options"), "psi/advanced")
{
    w = nullptr;
}

OptionsTabAdvanced::~OptionsTabAdvanced() {}

QWidget *OptionsTabAdvanced::widget()
{
    if (w)
        return nullptr;

    w                = new OptAdvancedUI();
    OptAdvancedUI *d = static_cast<OptAdvancedUI *>(w);

#ifdef HAVE_X11 // auto-copy is a built-in feature on linux, we don't want user to use our own one
    d->ck_autocopy->hide();
#endif

    d->ck_messageevents->setToolTip(tr("Enables the sending and requesting of message events such as "
                                       "'Contact is Typing', ..."));
    d->ck_inactiveevents->setToolTip(tr("Enables the sending of events when you end or suspend a "
                                        "conversation"));
    d->ck_requestReceipts->setToolTip(tr("Request receipts from contacts on each message."));
    d->ck_sendReceipts->setToolTip(tr("Send receipts to contacts by request."));
    d->ck_rc->setToolTip(tr("Enables remote controlling your client from other locations"));
    d->ck_autocopy->setToolTip(tr("Check this option if you want the selected text in incoming messages and chat log "
                                  "to be automatically copied to clipboard"));
    d->ck_singleclick->setToolTip(tr("Normally, a double-click on a contact will invoke the default action."
                                     "  Check this option if you'd rather invoke with a single-click."));
    d->ck_jidComplete->setToolTip(tr("Enables as-you-type JID autocompletion in message dialog."));
    d->ck_grabUrls->setToolTip(tr("Automatically attaches URLs from clipboard to the messages when enabled"));
    d->cb_incomingAs->setToolTip(
        tr("<P>Specifies how to treat incoming events:</P>"
           "<P><B>Normal</B> - messages come as messages, chats come as chats.</P>"
           "<P><B>Messages</B> - All messages/chats come as messages, no matter what their original form was.</P>"
           "<P><B>Chats</B> - All messages/chats come as chats, no matter what their original form was.</P>"));

    d->cb_incomingAs->setItemData(0, "no");
    d->cb_incomingAs->setItemData(1, "message");
    d->cb_incomingAs->setItemData(2, "chat");
    d->cb_incomingAs->setItemData(3, "current-open");
    d->ck_showSubjects->setToolTip(
        tr("Makes Psi show separate subject line in messages. Uncheck this if you want to save some screen space."));
    d->ck_autoVCardOnLogin->setToolTip(tr("By default, Psi always checks your vCard on login. If you want to save "
                                          "some traffic, you can uncheck this option."));
    d->ck_rosterAnim->setToolTip(tr("Makes Psi animate contact names in the main window when they come online."));
    d->ck_scrollTo->setToolTip(
        tr("Makes Psi scroll the main window automatically so you can see new incoming events."));
    d->ck_ignoreHeadline->setToolTip(tr("Makes Psi ignore all incoming \"headline\" events,"
                                        " like system-wide news on MSN, announcements, etc."));

    connect(d->ck_messageevents, SIGNAL(toggled(bool)), d->ck_inactiveevents, SLOT(setEnabled(bool)));
    connect(d->ck_messageevents, SIGNAL(toggled(bool)), d->ck_sendComposingEvents, SLOT(setEnabled(bool)));
    d->ck_inactiveevents->setEnabled(d->ck_messageevents->isChecked());
    d->ck_sendComposingEvents->setEnabled(d->ck_messageevents->isChecked());

    return w;
}

void OptionsTabAdvanced::applyOptions()
{
    if (!w)
        return;

    OptAdvancedUI *d = static_cast<OptAdvancedUI *>(w);

    PsiOptions::instance()->setOption("options.messages.send-composing-events", d->ck_messageevents->isChecked());
    PsiOptions::instance()->setOption("options.messages.send-inactivity-events", d->ck_inactiveevents->isChecked());
    PsiOptions::instance()->setOption("options.ui.notifications.request-receipts", d->ck_requestReceipts->isChecked());
    PsiOptions::instance()->setOption("options.ui.notifications.send-receipts", d->ck_sendReceipts->isChecked());
    PsiOptions::instance()->setOption("options.messages.dont-send-composing-events",
                                      d->ck_sendComposingEvents->isChecked());
    PsiOptions::instance()->setOption("options.external-control.adhoc-remote-control.enable", d->ck_rc->isChecked());
    PsiOptions::instance()->setOption("options.ui.automatically-copy-selected-text", d->ck_autocopy->isChecked());
    PsiOptions::instance()->setOption("options.ui.contactlist.use-single-click", d->ck_singleclick->isChecked());
    PsiOptions::instance()->setOption("options.ui.message.use-jid-auto-completion", d->ck_jidComplete->isChecked());
    PsiOptions::instance()->setOption("options.ui.message.auto-grab-urls-from-clipboard", d->ck_grabUrls->isChecked());
    PsiOptions::instance()->setOption("options.messages.force-incoming-message-type",
                                      d->cb_incomingAs->itemData(d->cb_incomingAs->currentIndex()));
    PsiOptions::instance()->setOption("options.ui.message.show-subjects", d->ck_showSubjects->isChecked());
    PsiOptions::instance()->setOption("options.vcard.query-own-vcard-on-login", d->ck_autoVCardOnLogin->isChecked());
    PsiOptions::instance()->setOption("options.ui.contactlist.use-status-change-animation",
                                      d->ck_rosterAnim->isChecked());
    PsiOptions::instance()->setOption("options.ui.contactlist.ensure-contact-visible-on-event",
                                      d->ck_scrollTo->isChecked());
    PsiOptions::instance()->setOption("options.messages.ignore-headlines", d->ck_ignoreHeadline->isChecked());
}

void OptionsTabAdvanced::restoreOptions()
{
    if (!w)
        return;

    OptAdvancedUI *d = static_cast<OptAdvancedUI *>(w);

    d->ck_messageevents->setChecked(
        PsiOptions::instance()->getOption("options.messages.send-composing-events").toBool());
    d->ck_inactiveevents->setChecked(
        PsiOptions::instance()->getOption("options.messages.send-inactivity-events").toBool());
    d->ck_requestReceipts->setChecked(
        PsiOptions::instance()->getOption("options.ui.notifications.request-receipts").toBool());
    d->ck_sendReceipts->setChecked(
        PsiOptions::instance()->getOption("options.ui.notifications.send-receipts").toBool());
    d->ck_sendComposingEvents->setChecked(
        PsiOptions::instance()->getOption("options.messages.dont-send-composing-events").toBool());
    d->ck_rc->setChecked(
        PsiOptions::instance()->getOption("options.external-control.adhoc-remote-control.enable").toBool());
    d->ck_autocopy->setChecked(
        PsiOptions::instance()->getOption("options.ui.automatically-copy-selected-text").toBool());
    d->ck_singleclick->setChecked(
        PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool());
    d->ck_jidComplete->setChecked(
        PsiOptions::instance()->getOption("options.ui.message.use-jid-auto-completion").toBool());
    d->ck_grabUrls->setChecked(
        PsiOptions::instance()->getOption("options.ui.message.auto-grab-urls-from-clipboard").toBool());
    d->cb_incomingAs->setCurrentIndex(d->cb_incomingAs->findData(
        PsiOptions::instance()->getOption("options.messages.force-incoming-message-type").toString()));
    d->ck_showSubjects->setChecked(PsiOptions::instance()->getOption("options.ui.message.show-subjects").toBool());
    d->ck_autoVCardOnLogin->setChecked(
        PsiOptions::instance()->getOption("options.vcard.query-own-vcard-on-login").toBool());
    d->ck_rosterAnim->setChecked(
        PsiOptions::instance()->getOption("options.ui.contactlist.use-status-change-animation").toBool());
    d->ck_scrollTo->setChecked(
        PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool());
    d->ck_ignoreHeadline->setChecked(PsiOptions::instance()->getOption("options.messages.ignore-headlines").toBool());
}
