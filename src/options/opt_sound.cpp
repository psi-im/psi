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

    sounds_ << UiSoundItem { d->ck_oeMessage,
                             d->le_oeMessage,
                             d->tb_seMessage,
                             d->tb_seMessagePlay,
                             QStringLiteral("incoming-message"),
                             QStringLiteral("sound/chat2.wav") };

    sounds_ << UiSoundItem { d->ck_oeChat1,
                             d->le_oeChat1,
                             d->tb_seChat1,
                             d->tb_seChat1Play,
                             QStringLiteral("new-chat"),
                             QStringLiteral("sound/chat1.wav") };

    sounds_ << UiSoundItem { d->ck_oeChat2,
                             d->le_oeChat2,
                             d->tb_seChat2,
                             d->tb_seChat2Play,
                             QStringLiteral("chat-message"),
                             QStringLiteral("sound/chat2.wav") };

    sounds_ << UiSoundItem { d->ck_oeGroupChat,
                             d->le_oeGroupChat,
                             d->tb_seGroupChat,
                             d->tb_seGroupChatPlay,
                             QStringLiteral("groupchat-message"),
                             QStringLiteral("sound/chat2.wav") };

    sounds_ << UiSoundItem { d->ck_oeHeadline,
                             d->le_oeHeadline,
                             d->tb_seHeadline,
                             d->tb_seHeadlinePlay,
                             QStringLiteral("incoming-headline"),
                             QStringLiteral("sound/chat2.wav") };

    sounds_ << UiSoundItem { d->ck_oeSystem,
                             d->le_oeSystem,
                             d->tb_seSystem,
                             d->tb_seSystemPlay,
                             QStringLiteral("system-message"),
                             QStringLiteral("sound/chat2.wav") };

    sounds_ << UiSoundItem { d->ck_oeOnline,
                             d->le_oeOnline,
                             d->tb_seOnline,
                             d->tb_seOnlinePlay,
                             QStringLiteral("contact-online"),
                             QStringLiteral("sound/online.wav") };

    sounds_ << UiSoundItem { d->ck_oeOffline,
                             d->le_oeOffline,
                             d->tb_seOffline,
                             d->tb_seOfflinePlay,
                             QStringLiteral("contact-offline"),
                             QStringLiteral("sound/offline.wav") };

    sounds_ << UiSoundItem { d->ck_oeSend,
                             d->le_oeSend,
                             d->tb_seSend,
                             d->tb_seSendPlay,
                             QStringLiteral("outgoing-chat"),
                             QStringLiteral("sound/send.wav") };

    sounds_ << UiSoundItem { d->ck_oeIncomingFT,
                             d->le_oeIncomingFT,
                             d->tb_seIncomingFT,
                             d->tb_seIncomingFTPlay,
                             QStringLiteral("incoming-file-transfer"),
                             QStringLiteral("sound/ft_incoming.wav") };

    sounds_ << UiSoundItem { d->ck_oeFTComplete,
                             d->le_oeFTComplete,
                             d->tb_seFTComplete,
                             d->tb_seFTCompletePlay,
                             QStringLiteral("completed-file-transfer"),
                             QStringLiteral("sound/ft_complete.wav") };

    bg_se     = new QButtonGroup;
    bg_sePlay = new QButtonGroup;

    for (auto const &sound : std::as_const(sounds_)) {
        bg_se->addButton(sound.selectButton);
        modify_buttons_[sound.selectButton] = sound.file;
        bg_sePlay->addButton(sound.playButton);
        play_buttons_[sound.playButton] = sound.file;

        // set up proper tool button icons
        IconToolButton *tb = static_cast<IconToolButton *>(sound.selectButton);
        tb->setPsiIcon(IconsetFactory::iconPtr("psi/browse"));
        tb = static_cast<IconToolButton *>(sound.playButton);
        tb->setPsiIcon(IconsetFactory::iconPtr("psi/play"));

        // TODO: add ToolTip for earch widget
    }

    connect(bg_se, qOverload<QAbstractButton *>(&QButtonGroup::buttonClicked), this,
            &OptionsTabSound::chooseSoundEvent);
    connect(bg_sePlay, qOverload<QAbstractButton *>(&QButtonGroup::buttonClicked), this,
            &OptionsTabSound::previewSoundEvent);
    connect(d->pb_soundReset, &QPushButton::clicked, this, &OptionsTabSound::soundReset);

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
    for (auto const &ui : std::as_const(sounds_)) {
        ui.file->setText(ui.defaultFile);
        ui.enabled->setChecked(true);
    }
    emit dataChanged();
}
