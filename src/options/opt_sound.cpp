#include "opt_sound.h"

#include "common.h"
#include "fileutil.h"
#include "iconset.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "ui_opt_sound.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

class OptSoundUI : public QWidget, public Ui::OptSound {
public:
    OptSoundUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabSound
//----------------------------------------------------------------------------

OptionsTabSound::OptionsTabSound(QObject *parent) :
    OptionsTab(parent, "sound", "", tr("Sound"), tr("Configure how Psi sounds"), "psi/playSounds")
{
}

OptionsTabSound::~OptionsTabSound()
{
    if (bg_se)
        delete bg_se;
    if (bg_sePlay)
        delete bg_sePlay;
}

QWidget *OptionsTabSound::widget()
{
    if (w)
        return nullptr;

    w             = new OptSoundUI();
    OptSoundUI *d = static_cast<OptSoundUI *>(w);

    bg_se = new QButtonGroup;

    bg_se->addButton(d->tb_seMessage);
    modify_buttons_[d->tb_seMessage] = d->le_oeMessage;
    sounds_ << UiSoundItem { d->ck_oeMessage, d->le_oeMessage, d->tb_seMessage, QStringLiteral("incoming-message") };

    bg_se->addButton(d->tb_seChat1);
    modify_buttons_[d->tb_seChat1] = d->le_oeChat1;
    sounds_ << UiSoundItem { d->ck_oeChat1, d->le_oeChat1, d->tb_seChat1, QStringLiteral("new-chat") };

    bg_se->addButton(d->tb_seChat2);
    modify_buttons_[d->tb_seChat2] = d->le_oeChat2;
    sounds_ << UiSoundItem { d->ck_oeChat2, d->le_oeChat2, d->tb_seChat2, QStringLiteral("chat-message") };

    bg_se->addButton(d->tb_seGroupChat);
    modify_buttons_[d->tb_seGroupChat] = d->le_oeGroupChat;
    sounds_ << UiSoundItem { d->ck_oeGroupChat, d->le_oeGroupChat, d->tb_seGroupChat,
                             QStringLiteral("groupchat-message") };

    bg_se->addButton(d->tb_seHeadline);
    modify_buttons_[d->tb_seHeadline] = d->le_oeHeadline;
    sounds_ << UiSoundItem { d->ck_oeHeadline, d->le_oeHeadline, d->tb_seHeadline,
                             QStringLiteral("incoming-headline") };

    bg_se->addButton(d->tb_seSystem);
    modify_buttons_[d->tb_seSystem] = d->le_oeSystem;
    sounds_ << UiSoundItem { d->ck_oeSystem, d->le_oeSystem, d->tb_seSystem, QStringLiteral("system-message") };

    bg_se->addButton(d->tb_seOnline);
    modify_buttons_[d->tb_seOnline] = d->le_oeOnline;
    sounds_ << UiSoundItem { d->ck_oeOnline, d->le_oeOnline, d->tb_seOnline, QStringLiteral("contact-online") };

    bg_se->addButton(d->tb_seOffline);
    modify_buttons_[d->tb_seOffline] = d->le_oeOffline;
    sounds_ << UiSoundItem { d->ck_oeOffline, d->le_oeOffline, d->tb_seOffline, QStringLiteral("contact-offline") };

    bg_se->addButton(d->tb_seSend);
    modify_buttons_[d->tb_seSend] = d->le_oeSend;
    sounds_ << UiSoundItem { d->ck_oeSend, d->le_oeSend, d->tb_seSend, QStringLiteral("outgoing-chat") };

    bg_se->addButton(d->tb_seIncomingFT);
    modify_buttons_[d->tb_seIncomingFT] = d->le_oeIncomingFT;
    sounds_ << UiSoundItem { d->ck_oeIncomingFT, d->le_oeIncomingFT, d->tb_seIncomingFT,
                             QStringLiteral("incoming-file-transfer") };

    bg_se->addButton(d->tb_seFTComplete);
    modify_buttons_[d->tb_seFTComplete] = d->le_oeFTComplete;
    sounds_ << UiSoundItem { d->ck_oeFTComplete, d->le_oeFTComplete, d->tb_seFTComplete,
                             QStringLiteral("completed-file-transfer") };

    connect(bg_se, SIGNAL(buttonClicked(QAbstractButton *)), SLOT(chooseSoundEvent(QAbstractButton *)));

    bg_sePlay = new QButtonGroup;
    bg_sePlay->addButton(d->tb_seMessagePlay);
    play_buttons_[d->tb_seMessagePlay] = d->le_oeMessage;
    bg_sePlay->addButton(d->tb_seChat1Play);
    play_buttons_[d->tb_seChat1Play] = d->le_oeChat1;
    bg_sePlay->addButton(d->tb_seChat2Play);
    play_buttons_[d->tb_seChat2Play] = d->le_oeChat2;
    bg_sePlay->addButton(d->tb_seGroupChatPlay);
    play_buttons_[d->tb_seGroupChatPlay] = d->le_oeGroupChat;
    bg_sePlay->addButton(d->tb_seHeadlinePlay);
    play_buttons_[d->tb_seHeadlinePlay] = d->le_oeHeadline;
    bg_sePlay->addButton(d->tb_seSystemPlay);
    play_buttons_[d->tb_seSystemPlay] = d->le_oeSystem;
    bg_sePlay->addButton(d->tb_seOnlinePlay);
    play_buttons_[d->tb_seOnlinePlay] = d->le_oeOnline;
    bg_sePlay->addButton(d->tb_seOfflinePlay);
    play_buttons_[d->tb_seOfflinePlay] = d->le_oeOffline;
    bg_sePlay->addButton(d->tb_seSendPlay);
    play_buttons_[d->tb_seSendPlay] = d->le_oeSend;
    bg_sePlay->addButton(d->tb_seIncomingFTPlay);
    play_buttons_[d->tb_seIncomingFTPlay] = d->le_oeIncomingFT;
    bg_sePlay->addButton(d->tb_seFTCompletePlay);
    play_buttons_[d->tb_seFTCompletePlay] = d->le_oeFTComplete;
    connect(bg_sePlay, SIGNAL(buttonClicked(QAbstractButton *)), SLOT(previewSoundEvent(QAbstractButton *)));

    connect(d->pb_soundReset, SIGNAL(clicked()), SLOT(soundReset()));

    // set up proper tool button icons
    int n;
    for (n = 0; n < 11; n++) {
        IconToolButton *tb = static_cast<IconToolButton *>(bg_se->buttons().at(n));
        tb->setPsiIcon(IconsetFactory::iconPtr("psi/browse"));
        tb = static_cast<IconToolButton *>(bg_sePlay->buttons().at(n));
        tb->setPsiIcon(IconsetFactory::iconPtr("psi/play"));
    }

    // TODO: add ToolTip for earch widget

    d->le_player->setToolTip(tr("If your system supports multiple sound players, you may"
                                " choose your preferred sound player application here."));
    d->ck_awaySound->setToolTip(
        tr("Enable this option if you wish to hear sound alerts when your status is \"away\" or \"extended away\"."));
    d->ck_gcSound->setToolTip(tr("Play sounds for all events in groupchat, not only for mentioning of your nick."));

#if defined(Q_OS_WIN)
    d->lb_player->hide();
    d->le_player->hide();
#elif defined(Q_OS_MAC)
    d->lb_player->hide();
    d->le_player->hide();
#endif

    return w;
}

void OptionsTabSound::applyOptions()
{
    if (!w)
        return;

    OptSoundUI *d = static_cast<OptSoundUI *>(w);
    PsiOptions::instance()->setOption("options.ui.notifications.sounds.unix-sound-player", d->le_player->text());
    PsiOptions::instance()->setOption("options.ui.notifications.sounds.silent-while-away",
                                      !d->ck_awaySound->isChecked());
    PsiOptions::instance()->setOption("options.ui.notifications.sounds.notify-every-muc-message",
                                      d->ck_gcSound->isChecked());

    for (auto const &ui : std::as_const(sounds_)) {
        auto opt   = QLatin1String("options.ui.notifications.sounds.") + ui.option;
        auto value = ui.file->text();
        if (!value.isEmpty() && !ui.enabled->isChecked()) {
            value = "-" + value;
        }
        PsiOptions::instance()->setOption(opt, value);
    }
}

void OptionsTabSound::restoreOptions()
{
    if (!w)
        return;

    OptSoundUI *d = static_cast<OptSoundUI *>(w);

#if defined(Q_OS_WIN)
    d->le_player->setText(tr("Windows Sound"));
#elif defined(Q_OS_MAC)
    d->le_player->setText(tr("Mac OS Sound"));
#else
    d->le_player->setText(
        PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString());
#endif

    d->ck_awaySound->setChecked(
        !PsiOptions::instance()->getOption("options.ui.notifications.sounds.silent-while-away").toBool());
    d->ck_gcSound->setChecked(
        PsiOptions::instance()->getOption("options.ui.notifications.sounds.notify-every-muc-message").toBool());

    for (auto const &ui : std::as_const(sounds_)) {
        auto opt   = QLatin1String("options.ui.notifications.sounds.") + ui.option;
        auto value = PsiOptions::instance()->getOption(opt).toString();
        if (value.startsWith("-")) {
            ui.enabled->setChecked(false);
            ui.file->setText(value.mid(1));
        } else {
            ui.enabled->setChecked(true);
            ui.file->setText(value);
        }
    }
}

void OptionsTabSound::setData(PsiCon *, QWidget *p) { parentWidget = p; }

bool OptionsTabSound::stretchable() const { return true; }

void OptionsTabSound::chooseSoundEvent(QAbstractButton *b)
{
    QString str = FileUtil::getOpenFileName(parentWidget, tr("Choose a sound file"), tr("Sound (*.wav)"));
    if (!str.isEmpty()) {
        modify_buttons_[b]->setText(str);
        emit dataChanged();
    }
}

void OptionsTabSound::previewSoundEvent(QAbstractButton *b) { soundPlay(play_buttons_[b]->text()); }

void OptionsTabSound::soundReset()
{
    OptSoundUI *d = static_cast<OptSoundUI *>(w);

    d->le_oeMessage->setText("sound/chat2.wav");
    d->ck_oeMessage->setChecked(true);
    d->le_oeChat1->setText("sound/chat1.wav");
    d->ck_oeChat1->setChecked(true);
    d->le_oeChat2->setText("sound/chat2.wav");
    d->ck_oeChat2->setChecked(true);
    d->le_oeGroupChat->setText("sound/chat2.wav");
    d->ck_oeGroupChat->setChecked(true);
    d->le_oeSystem->setText("sound/chat2.wav");
    d->ck_oeSystem->setChecked(true);
    d->le_oeHeadline->setText("sound/chat2.wav");
    d->ck_oeHeadline->setChecked(true);
    d->le_oeOnline->setText("sound/online.wav");
    d->ck_oeOnline->setChecked(true);
    d->le_oeOffline->setText("sound/offline.wav");
    d->ck_oeOffline->setChecked(true);
    d->le_oeSend->setText("sound/send.wav");
    d->ck_oeSend->setChecked(true);
    d->le_oeIncomingFT->setText("sound/ft_incoming.wav");
    d->ck_oeIncomingFT->setChecked(true);
    d->le_oeFTComplete->setText("sound/ft_complete.wav");
    d->ck_oeFTComplete->setChecked(true);
    emit dataChanged();
}
