#include "opt_sound.h"

#include <QButtonGroup>
#include <QWhatsThis>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QLabel>

#include "common.h"
#include "iconwidget.h"
#include "iconset.h"
#include "applicationinfo.h"
#include "psioptions.h"
#include "fileutil.h"

#include "ui_opt_sound.h"

class OptSoundUI : public QWidget, public Ui::OptSound
{
public:
	OptSoundUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabSound
//----------------------------------------------------------------------------

OptionsTabSound::OptionsTabSound(QObject *parent)
: OptionsTab(parent, "sound", "", tr("Sound"), tr("Configure how Psi sounds"), "psi/playSounds")
{
	w = 0;
	bg_se = bg_sePlay = 0;
}

OptionsTabSound::~OptionsTabSound()
{
	if ( bg_se )
		delete bg_se;
	if ( bg_sePlay )
		delete bg_sePlay;
}

QWidget *OptionsTabSound::widget()
{
	if ( w )
		return 0;

	w = new OptSoundUI();
	OptSoundUI *d = (OptSoundUI *)w;

	sounds_ << d->le_oeMessage << d->le_oeChat1 << d->le_oeChat2 << d->le_oeHeadline << d->le_oeSystem << d->le_oeOnline << d->le_oeOffline << d->le_oeSend << d->le_oeIncomingFT << d->le_oeFTComplete;

	bg_se = new QButtonGroup;
	bg_se->addButton(d->tb_seMessage);
	modify_buttons_[d->tb_seMessage] = d->le_oeMessage;
	bg_se->addButton(d->tb_seChat1);
	modify_buttons_[d->tb_seChat1] = d->le_oeChat1;
	bg_se->addButton(d->tb_seChat2);
	modify_buttons_[d->tb_seChat2] = d->le_oeChat2;
	bg_se->addButton(d->tb_seHeadline);
	modify_buttons_[d->tb_seHeadline] = d->le_oeHeadline;
	bg_se->addButton(d->tb_seSystem);
	modify_buttons_[d->tb_seSystem] = d->le_oeSystem;
	bg_se->addButton(d->tb_seOnline);
	modify_buttons_[d->tb_seOnline] = d->le_oeOnline;
	bg_se->addButton(d->tb_seOffline);
	modify_buttons_[d->tb_seOffline] = d->le_oeOffline;
	bg_se->addButton(d->tb_seSend);
	modify_buttons_[d->tb_seSend] = d->le_oeSend;
	bg_se->addButton(d->tb_seIncomingFT);
	modify_buttons_[d->tb_seIncomingFT] = d->le_oeIncomingFT;
	bg_se->addButton(d->tb_seFTComplete);
	modify_buttons_[d->tb_seFTComplete] = d->le_oeFTComplete;
	connect(bg_se, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(chooseSoundEvent(QAbstractButton*)));

	bg_sePlay = new QButtonGroup;
	bg_sePlay->addButton(d->tb_seMessagePlay);
	play_buttons_[d->tb_seMessagePlay] = d->le_oeMessage;
	bg_sePlay->addButton(d->tb_seChat1Play);
	play_buttons_[d->tb_seChat1Play] = d->le_oeChat1;
	bg_sePlay->addButton(d->tb_seChat2Play);
	play_buttons_[d->tb_seChat2Play] = d->le_oeChat2;
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
	connect(bg_sePlay, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(previewSoundEvent(QAbstractButton*)));

	connect(d->pb_soundReset, SIGNAL(clicked()), SLOT(soundReset()));

	// set up proper tool button icons
	int n;
	for (n = 0; n < 10; n++) {
		IconToolButton *tb = (IconToolButton *)bg_se->buttons()[n];
		tb->setPsiIcon( IconsetFactory::iconPtr("psi/browse") );
		tb = (IconToolButton *)bg_sePlay->buttons()[n];
		tb->setPsiIcon( IconsetFactory::iconPtr("psi/play") );
	}

	// TODO: add QWhatsThis for all widgets

	d->le_player->setWhatsThis(
		tr("If your system supports multiple sound players, you may"
		" choose your preferred sound player application here."));
	d->ck_awaySound->setWhatsThis(
		tr("Enable this option if you wish to hear sound alerts when your status is \"away\" or \"extended away\"."));
	d->ck_gcSound->setWhatsThis(
		tr("Play sounds for all events in groupchat, not only for mentioning of your nick."));

#if defined(Q_WS_WIN)
	d->lb_player->hide();
	d->le_player->hide();
#elif defined(Q_WS_MAC)
	d->lb_player->hide();
	d->le_player->hide();
#endif

	return w;
}

void OptionsTabSound::applyOptions()
{
	if ( !w )
		return;

	OptSoundUI *d = (OptSoundUI *)w;
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.unix-sound-player", d->le_player->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.silent-while-away", !d->ck_awaySound->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.notify-every-muc-message", d->ck_gcSound->isChecked());
	
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.incoming-message", d->le_oeMessage->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.new-chat", d->le_oeChat1->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.chat-message", d->le_oeChat2->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.system-message", d->le_oeSystem->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.incoming-headline", d->le_oeHeadline->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.contact-online", d->le_oeOnline->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.contact-offline", d->le_oeOffline->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.outgoing-chat", d->le_oeSend->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.incoming-file-transfer", d->le_oeIncomingFT->text());
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.completed-file-transfer", d->le_oeFTComplete->text());
	
}

void OptionsTabSound::restoreOptions()
{
	if ( !w )
		return;

	OptSoundUI *d = (OptSoundUI *)w;

#if defined(Q_WS_WIN)
	d->le_player->setText(tr("Windows Sound"));
#elif defined(Q_WS_MAC)
	d->le_player->setText(tr("Mac OS Sound"));
#else
	d->le_player->setText( PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString() );
#endif

	d->ck_awaySound->setChecked( !PsiOptions::instance()->getOption("options.ui.notifications.sounds.silent-while-away").toBool() );
	d->ck_gcSound->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.sounds.notify-every-muc-message").toBool() );

	d->le_oeMessage->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-message").toString());
	d->le_oeChat1->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.new-chat").toString());
	d->le_oeChat2->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.chat-message").toString());
	d->le_oeSystem->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.system-message").toString());
	d->le_oeHeadline->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-headline").toString());
	d->le_oeOnline->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.contact-online").toString());
	d->le_oeOffline->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.contact-offline").toString());
	d->le_oeSend->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.outgoing-chat").toString());
	d->le_oeIncomingFT->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-file-transfer").toString());
	d->le_oeFTComplete->setText(PsiOptions::instance()->getOption("options.ui.notifications.sounds.completed-file-transfer").toString());
}

void OptionsTabSound::setData(PsiCon *, QWidget *p)
{
	parentWidget = p;
}

void OptionsTabSound::chooseSoundEvent(QAbstractButton* b)
{
	QString str = FileUtil::getOpenFileName(parentWidget,
	                                        tr("Choose a sound file"),
	                                        tr("Sound (*.wav)"));
	if (!str.isEmpty()) {
		modify_buttons_[b]->setText(str);
		emit dataChanged();
	}
}

void OptionsTabSound::previewSoundEvent(QAbstractButton* b)
{
	soundPlay(play_buttons_[b]->text());
}

void OptionsTabSound::soundReset()
{
	OptSoundUI *d = (OptSoundUI *)w;
	
	d->le_oeMessage->setText("sound/chat2.wav");
	d->le_oeChat1->setText("sound/chat1.wav");
	d->le_oeChat2->setText("sound/chat2.wav");
	d->le_oeSystem->setText("sound/chat2.wav");
	d->le_oeHeadline->setText("sound/chat2.wav");
	d->le_oeOnline->setText("sound/online.wav");
	d->le_oeOffline->setText("sound/offline.wav");
	d->le_oeSend->setText("sound/send.wav");
	d->le_oeIncomingFT->setText("sound/ft_incoming.wav");
	d->le_oeFTComplete->setText("sound/ft_complete.wav");
	emit dataChanged();
}
