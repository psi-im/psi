#include "opt_sound.h"
#include "common.h"
#include "iconwidget.h"
#include "iconset.h"
#include "applicationinfo.h"

#include <qbuttongroup.h>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qfiledialog.h>
#include <qlabel.h>

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
	bg_se->insert(d->tb_seMessage);
	modify_buttons_[d->tb_seMessage] = d->le_oeMessage;
	bg_se->insert(d->tb_seChat1);
	modify_buttons_[d->tb_seChat1] = d->le_oeChat1;
	bg_se->insert(d->tb_seChat2);
	modify_buttons_[d->tb_seChat2] = d->le_oeChat2;
	bg_se->insert(d->tb_seHeadline);
	modify_buttons_[d->tb_seHeadline] = d->le_oeHeadline;
	bg_se->insert(d->tb_seSystem);
	modify_buttons_[d->tb_seSystem] = d->le_oeSystem;
	bg_se->insert(d->tb_seOnline);
	modify_buttons_[d->tb_seOnline] = d->le_oeOnline;
	bg_se->insert(d->tb_seOffline);
	modify_buttons_[d->tb_seOffline] = d->le_oeOffline;
	bg_se->insert(d->tb_seSend);
	modify_buttons_[d->tb_seSend] = d->le_oeSend;
	bg_se->insert(d->tb_seIncomingFT);
	modify_buttons_[d->tb_seIncomingFT] = d->le_oeIncomingFT;
	bg_se->insert(d->tb_seFTComplete);
	modify_buttons_[d->tb_seFTComplete] = d->le_oeFTComplete;
	connect(bg_se, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(chooseSoundEvent(QAbstractButton*)));

	bg_sePlay = new QButtonGroup;
	bg_sePlay->insert(d->tb_seMessagePlay);
	play_buttons_[d->tb_seMessagePlay] = d->le_oeMessage;
	bg_sePlay->insert(d->tb_seChat1Play);
	play_buttons_[d->tb_seChat1Play] = d->le_oeChat1;
	bg_sePlay->insert(d->tb_seChat2Play);
	play_buttons_[d->tb_seChat2Play] = d->le_oeChat2;
	bg_sePlay->insert(d->tb_seHeadlinePlay);
	play_buttons_[d->tb_seHeadlinePlay] = d->le_oeHeadline;
	bg_sePlay->insert(d->tb_seSystemPlay);
	play_buttons_[d->tb_seSystemPlay] = d->le_oeSystem;
	bg_sePlay->insert(d->tb_seOnlinePlay);
	play_buttons_[d->tb_seOnlinePlay] = d->le_oeOnline;
	bg_sePlay->insert(d->tb_seOfflinePlay);
	play_buttons_[d->tb_seOfflinePlay] = d->le_oeOffline;
	bg_sePlay->insert(d->tb_seSendPlay);
	play_buttons_[d->tb_seSendPlay] = d->le_oeSend;
	bg_sePlay->insert(d->tb_seIncomingFTPlay);
	play_buttons_[d->tb_seIncomingFTPlay] = d->le_oeIncomingFT;
	bg_sePlay->insert(d->tb_seFTCompletePlay);
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

	QWhatsThis::add(d->le_player,
		tr("If your system supports multiple sound players, you may"
		" choose your preferred sound player application here."));
	QWhatsThis::add(d->ck_awaySound,
		tr("Enable this option if you wish to hear sound alerts when your status is \"away\" or \"extended away\"."));
	QWhatsThis::add(d->ck_gcSound,
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

void OptionsTabSound::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptSoundUI *d = (OptSoundUI *)w;
	opt->player = d->le_player->text();
	opt->noAwaySound = !d->ck_awaySound->isChecked();
	opt->noGCSound = !d->ck_gcSound->isChecked();

	int n = 0;
	foreach(QLineEdit* le, sounds_) {
		opt->onevent[n++] = le->text();
	}
}

void OptionsTabSound::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptSoundUI *d = (OptSoundUI *)w;

#if defined(Q_WS_WIN)
	d->le_player->setText(tr("Windows Sound"));
#elif defined(Q_WS_MAC)
	d->le_player->setText(tr("Mac OS Sound"));
#else
	d->le_player->setText( opt->player );
#endif

	d->ck_awaySound->setChecked( !opt->noAwaySound );
	d->ck_gcSound->setChecked( !opt->noGCSound );

	int n = 0;
	foreach(QLineEdit* le, sounds_) {
		le->setText(opt->onevent[n++]);
	}
}

void OptionsTabSound::setData(PsiCon *, QWidget *p)
{
	parentWidget = p;
}

void OptionsTabSound::chooseSoundEvent(QAbstractButton* b)
{
	if(option.lastPath.isEmpty())
		option.lastPath = QDir::homeDirPath();
	QString str = QFileDialog::getOpenFileName(option.lastPath, tr("Sound (*.wav)"), parentWidget, "", tr("Choose a sound file"));
	if(!str.isEmpty()) {
		QFileInfo fi(str);
		option.lastPath = fi.dirPath();
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
	Options opt;
	opt.onevent[eMessage]    = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	opt.onevent[eChat1]      = ApplicationInfo::resourcesDir() + "/sound/chat1.wav";
	opt.onevent[eChat2]      = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	opt.onevent[eHeadline]   = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	opt.onevent[eSystem]     = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	opt.onevent[eOnline]     = ApplicationInfo::resourcesDir() + "/sound/online.wav";
	opt.onevent[eOffline]    = ApplicationInfo::resourcesDir() + "/sound/offline.wav";
	opt.onevent[eSend]       = ApplicationInfo::resourcesDir() + "/sound/send.wav";
	opt.onevent[eIncomingFT] = ApplicationInfo::resourcesDir() + "/sound/ft_incoming.wav";
	opt.onevent[eFTComplete] = ApplicationInfo::resourcesDir() + "/sound/ft_complete.wav";

	int n = 0;
	foreach(QLineEdit* le, sounds_) {
		le->setText(opt.onevent[n++]);
	}

	emit dataChanged();
}
