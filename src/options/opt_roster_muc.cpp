#include "opt_roster_muc.h"

#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "ui_opt_roster_muc.h"

class OptRosterMucUI : public QWidget, public Ui::OptRosterMuc {
public:
    OptRosterMucUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabRosterMuc
//----------------------------------------------------------------------------

OptionsTabRosterMuc::OptionsTabRosterMuc(QObject *parent) :
    OptionsTab(parent, "roster_muc", "", tr("Groupchat"), tr("Groupchat roster options"), "psi/groupChat"), w(nullptr)
{
}

OptionsTabRosterMuc::~OptionsTabRosterMuc() { }

void OptionsTabRosterMuc::changeEvent(QEvent *e)
{
    if (!w)
        return;

    OptRosterMucUI *d = static_cast<OptRosterMucUI *>(w);

    switch (e->type()) {
    case QEvent::LanguageChange:
        d->retranslateUi(w);
        break;
    default:
        break;
    }
}

QWidget *OptionsTabRosterMuc::widget()
{
    if (w)
        return nullptr;

    w                 = new OptRosterMucUI();
    OptRosterMucUI *d = static_cast<OptRosterMucUI *>(w);
    QObject::connect(d->ck_showAvatarIcons, &QCheckBox::toggled, this,
                     [d](bool checked) { d->gb_avatarsSettings->setEnabled(checked); });

    return w;
}

void OptionsTabRosterMuc::applyOptions()
{
    if (!w)
        return;

    OptRosterMucUI *d = static_cast<OptRosterMucUI *>(w);

    PsiOptions::instance()->setOption("options.ui.muc.userlist.disable-scrollbar", d->ck_disableScrollbar->isChecked());
    PsiOptions::instance()->setOption("options.ui.muc.roster-at-left", d->ck_rosterAtLeft->isChecked());

    // enabled icons
    PsiOptions::instance()->setOption("options.ui.muc.userlist.avatars.show", d->ck_showAvatarIcons->isChecked());
    PsiOptions::instance()->setOption("options.ui.muc.userlist.show-client-icons", d->ck_showClientIcons->isChecked());
    PsiOptions::instance()->setOption("options.ui.muc.userlist.show-status-icons", d->ck_showStatusIcons->isChecked());
    PsiOptions::instance()->setOption("options.ui.muc.userlist.show-affiliation-icons",
                                      d->ck_showAffiliationIcons->isChecked());

    // avatars settings
    PsiOptions::instance()->setOption("options.ui.muc.userlist.avatars.size", d->sb_avatarsSize->value());
    PsiOptions::instance()->setOption("options.ui.muc.userlist.avatars.radius", d->sb_avatarsRadius->value());
    PsiOptions::instance()->setOption("options.ui.muc.userlist.avatars.avatars-at-left",
                                      d->ck_avatarsAtLeft->isChecked());
}

void OptionsTabRosterMuc::restoreOptions()
{
    if (!w)
        return;

    OptRosterMucUI *d = static_cast<OptRosterMucUI *>(w);

    d->ck_disableScrollbar->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.disable-scrollbar").toBool());
    d->ck_rosterAtLeft->setChecked(PsiOptions::instance()->getOption("options.ui.muc.roster-at-left").toBool());

    // enabled icons
    d->ck_showAvatarIcons->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.avatars.show").toBool());
    d->ck_showClientIcons->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.show-client-icons").toBool());
    d->ck_showStatusIcons->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.show-status-icons").toBool());
    d->ck_showAffiliationIcons->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.show-affiliation-icons").toBool());
    // avatars settings
    d->sb_avatarsSize->setValue(PsiOptions::instance()->getOption("options.ui.muc.userlist.avatars.size").toInt());
    d->sb_avatarsRadius->setValue(PsiOptions::instance()->getOption("options.ui.muc.userlist.avatars.radius").toInt());
    d->ck_avatarsAtLeft->setChecked(
        PsiOptions::instance()->getOption("options.ui.muc.userlist.avatars.avatars-at-left").toBool());

    d->gb_avatarsSettings->setEnabled(d->ck_showAvatarIcons->isChecked());
}
