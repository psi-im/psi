#include "psichatdlg_b.h"

#include <QLabel>
#include <QCursor>
#include <QLineEdit>
#include <QToolButton>
#include <QLayout>
#include <QSplitter>
#include <QToolBar>
#include <QPixmap>
#include <QColor>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QMessageBox>

#include "psicon_b.h"
#include "psiaccount_b.h"
#include "common.h"
#include "iconaction.h"
#include "stretchwidget.h"
#include "psiiconset.h"
#include "iconwidget.h"
#include "fancylabel.h"
#include "msgmle.h"
#include "iconselect.h"
#include "avatars.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "accountlabel.h"
#include "iconlabel.h"
#include "capsmanager.h"
#include "psicontactlist.h"
#include "userlist.h"
#include "jidutil.h"
#include "textutil.h"
#include "vcardfactory.h"
#include "mainwin_p_b.h"
#include "cudaskin.h"
#include "invitedlg.h"

typedef QSet<ChatDlg*> ChatDlgSet;
Q_GLOBAL_STATIC(ChatDlgSet, chat_dialogs)

/*class PsiChatDlgPrivate
{
public:
	PsiChatDlgB *q;
	InviteDlg *invite;

	PsiChatDlgPrivate(PsiChatDlgB *_q) :
		q(_q),
		invite(0)
	{
	}
};

typedef QHash<PsiChatDlgB*,PsiChatDlgPrivate*> PsiChatDlgPrivateHash;
Q_GLOBAL_STATIC(PsiChatDlgPrivateHash, psichat_private)*/

// from mainwin.cpp
QImage makeAvatarImage(const QImage &_in);

PsiChatDlgB::PsiChatDlgB(const Jid& jid, PsiAccount* pa, TabManager* tabManager)
		: ChatDlg(jid, pa, tabManager)
{
	//psichat_private()->insert(this, new PsiChatDlgPrivate(this));
	chat_dialogs()->insert(this);

	connect(account()->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
}

PsiChatDlgB::~PsiChatDlgB()
{
	chat_dialogs()->remove(this);
	delete invite_;
	//delete psichat_private()->take(this);
}

void PsiChatDlgB::initUi()
{
	conversation_time_up = true;

	ui_.setupUi(this);

	/*QWidget *w = new QWidget(this);
	QVBoxLayout *w_vb = new QVBoxLayout(w);
	w_vb->setMargin(0);
	replaceWidget(ui_.lb_status, w);*/

	ui_.vboxLayout->setMargin(0);
	CudaSubFrame *tf = new CudaSubFrame(this);
	replaceWidget(ui_.lb_status, tf);
	//w_vb->addWidget(tf);
	ui_.le_jid->hide();
	ui_.lb_ident->hide();
	ui_.tb_actions->setParent(tf);
	ui_.log->setParent(tf);
	ui_.lb_count->hide();
	QHBoxLayout *hb = new QHBoxLayout(tf);
	hb->addWidget(ui_.log);
	QVBoxLayout *vb = new QVBoxLayout;
	hb->addLayout(vb);
	lb_peerpic = new QLabel(tf);
	vb->addWidget(lb_peerpic);
	//QHBoxLayout *hb2 = new QHBoxLayout;
	//hb2->setSizeConstraint(QLayout::SetFixedSize);
	//vb->addLayout(hb2);
	//hb2->addStretch(1);
	//hb2->addWidget(ui_.tb_actions);
	vb->addWidget(ui_.tb_actions);
	vb->addStretch(1);

	CudaSubFrame *bf = new CudaSubFrame(this);
	replaceWidget(ui_.avatar, bf);
	//w_vb->addWidget(bf);
	ui_.mle->setParent(bf);
	QVBoxLayout *bfvb = new QVBoxLayout(bf);
	hb = new QHBoxLayout;
	bfvb->addLayout(hb);
	QToolButton *tb_act_file = new QToolButton(bf);
	QToolButton *tb_act_call = new QToolButton(bf);
	tb_act_file->setIcon(get_icon_file());
	tb_act_call->setIcon(get_icon_call());
	hb->addWidget(tb_act_file);
	hb->addWidget(tb_act_call);
	hb->addStretch();
	hb = new QHBoxLayout;
	bfvb->addLayout(hb);
	hb->addWidget(ui_.mle);
	vb = new QVBoxLayout;
	hb->addLayout(vb);
	lb_selfpic = new ClickableLabel(bf);
	connect(lb_selfpic, SIGNAL(clicked()), SLOT(avatar_clicked()));
	vb->addWidget(lb_selfpic);
	vb->addStretch(1);

	QImage img;
	QPixmap avatarPixmap = account()->avatarFactory()->getAvatar(account()->jid());
	if(!avatarPixmap.isNull())
		img = avatarPixmap.toImage();
	lb_selfpic->setPixmap(QPixmap::fromImage(makeAvatarImage(img)));

	ui_.lb_ident->setAccount(account());
	ui_.lb_ident->setShowJid(false);

	//PsiToolTip::install(ui_.lb_status);
	//ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));

	//ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());

	connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit*)), SLOT(chatEditCreated()));
	chatEditCreated();

#ifdef Q_WS_MAC
	connect(chatView(), SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

	initToolButtons();
	initToolBar();
	updateAvatar();

	connect(tb_act_file, SIGNAL(clicked()), act_file_, SLOT(trigger()));
	connect(tb_act_call, SIGNAL(clicked()), act_voice_, SLOT(trigger()));

	UserListItem* u = account()->findFirstRelevant(jid());
	if (u && u->isSecure(jid().resource())) {
		setPGPEnabled(true);
	}

	connect(account()->avatarFactory(), SIGNAL(avatarChanged(const Jid&)), this, SLOT(updateAvatar(const Jid&)));

	pm_settings_ = new QMenu(this);
	connect(pm_settings_, SIGNAL(aboutToShow()), SLOT(buildMenu()));
	ui_.tb_actions->setMenu(pm_settings_);

	connect(account()->capsManager(), SIGNAL(capsChanged(const Jid&)), SLOT(capsChanged(const Jid&)));

	// ###cuda
	connect(account()->avatarFactory(), SIGNAL(avatarChanged(const Jid &)), SLOT(vcardChanged(const Jid &)));
	cuda_applyTheme(ui_.le_jid);
	cuda_applyTheme(ui_.log);
	cuda_applyTheme(ui_.mle);
	cuda_applyTheme(ui_.tb_actions);
	cuda_applyTheme(tb_act_file);
	cuda_applyTheme(tb_act_call);

	QList<int> list;
	list << 324;
	list << 96;
	ui_.splitter->setSizes(list);

	smallChat_ = false; //option.smallChats;
}

void PsiChatDlgB::updateCountVisibility()
{
	/*if (option.showCounter && !smallChat_) {
		ui_.lb_count->show();
	}
	else {
		ui_.lb_count->hide();
	}*/
}

void PsiChatDlgB::setLooks()
{
	ChatDlg::setLooks();

	ui_.splitter->optionsChanged();
	ui_.mle->optionsChanged();

	ui_.tb_pgp->hide();
	if (smallChat_) {
		//ui_.lb_status->hide();
		ui_.le_jid->hide();
		ui_.tb_actions->hide();
		ui_.tb_emoticons->hide();
		ui_.toolbar->hide();
	}
	else {
		//ui_.lb_status->show();
		//ui_.le_jid->show();
		if (PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool()) {
			ui_.toolbar->show();
			ui_.tb_actions->hide();
			ui_.tb_emoticons->hide();
		}
		else {
			ui_.toolbar->hide();
			//ui_.tb_emoticons->setVisible(option.useEmoticons);
			// ###cuda explicitly hide till we have a place for it
			ui_.tb_emoticons->setVisible(false);
			ui_.tb_actions->show();
		}
	}

	updateIdentityVisibility();
	updateCountVisibility();

	// toolbuttons
	QIcon i;
	//i.setPixmap(IconsetFactory::icon("psi/cryptoNo").impix(),  QIcon::Automatic, QIcon::Normal, QIcon::Off);
	//i.setPixmap(IconsetFactory::icon("psi/cryptoYes").impix(), QIcon::Automatic, QIcon::Normal, QIcon::On);
	act_pgp_->setPsiIcon(0);
	act_pgp_->setIcon(i);
}

void PsiChatDlgB::setShortcuts()
{
	ChatDlg::setShortcuts();

	act_clear_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
	act_info_->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
	act_history_->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));
}

void PsiChatDlgB::updateIdentityVisibility()
{
	if (!smallChat_) {
		bool visible = account()->psi()->contactList()->enabledAccounts().count() > 1;
		ui_.lb_ident->setVisible(visible);
	}
	else {
		ui_.lb_ident->setVisible(false);
	}
}

void PsiChatDlgB::initToolButtons()
{
	//act_clear_ = new IconAction(tr("Clear chat window"), "psi/clearChat", tr("Clear chat window"), 0, this);
	act_clear_ = new IconAction(tr("Clear chat window"), tr("Clear chat window"), 0, this);
	connect(act_clear_, SIGNAL(activated()), SLOT(doClearButton()));

	connect(account()->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), this, SLOT(addEmoticon(QString)));
	//act_icon_ = new IconAction(tr("Select icon"), "psi/smile", tr("Select icon"), 0, this);
	act_icon_ = new IconAction(tr("Select icon"), tr("Select icon"), 0, this);
	act_icon_->setMenu(account()->psi()->iconSelectPopup());
	ui_.tb_emoticons->setMenu(account()->psi()->iconSelectPopup());

	//act_voice_ = new IconAction(tr("Voice Call"), "psi/voice", tr("Voice Call"), 0, this);
	act_voice_ = new IconAction(tr("Voice Call"), tr("Voice Call"), 0, this);
	connect(act_voice_, SIGNAL(activated()), SLOT(doVoice()));
	//act_voice_->setEnabled(false);

	//act_file_ = new IconAction(tr("Send file"), "psi/upload", tr("Send file"), 0, this);
	act_file_ = new IconAction(tr("Send file"), tr("Send file"), 0, this);
	connect(act_file_, SIGNAL(activated()), SLOT(doFile()));

	//act_pgp_ = new IconAction(tr("Toggle encryption"), "psi/cryptoNo", tr("Toggle encryption"), 0, this, 0, true);
	act_pgp_ = new IconAction(tr("Toggle encryption"), tr("Toggle encryption"), 0, this, 0, true);
	ui_.tb_pgp->setDefaultAction(act_pgp_);

	//act_info_ = new IconAction(tr("User info"), "psi/vCard", tr("User info"), 0, this);
	act_info_ = new IconAction(tr("User info"), tr("User info"), 0, this);
	connect(act_info_, SIGNAL(activated()), SLOT(doInfo()));

	//act_history_ = new IconAction(tr("Message history"), "psi/history", tr("Message history"), 0, this);
	act_history_ = new IconAction(tr("Message history"), tr("Message history"), 0, this);
	connect(act_history_, SIGNAL(activated()), SLOT(doHistory()));

	//act_compact_ = new IconAction(tr("Toggle Compact/Full size"), "psi/compact", tr("Toggle Compact/Full size"), 0, this);
	act_compact_ = new IconAction(tr("Toggle Compact/Full size"), tr("Toggle Compact/Full size"), 0, this);
	connect(act_compact_, SIGNAL(activated()), SLOT(toggleSmallChat()));

	act_invite_ = new IconAction(tr("Invite..."), tr("Invite..."), 0, this);
	connect(act_invite_, SIGNAL(activated()), SLOT(doInvite()));
}

void PsiChatDlgB::initToolBar()
{
	ui_.toolbar->setWindowTitle(tr("Chat toolbar"));
	ui_.toolbar->setIconSize(QSize(16, 16));
	ui_.toolbar->addAction(act_clear_);
	ui_.toolbar->addWidget(new StretchWidget(ui_.toolbar));
	ui_.toolbar->addAction(act_icon_);
	ui_.toolbar->addAction(act_file_);
	if (PsiOptions::instance()->getOption("options.pgp.enable").toBool()) {
		ui_.toolbar->addAction(act_pgp_);
	}
	ui_.toolbar->addAction(act_info_);
	ui_.toolbar->addAction(act_history_);
	if (account()->voiceCaller()) {
		ui_.toolbar->addAction(act_voice_);
	}
}

void PsiChatDlgB::contextMenuEvent(QContextMenuEvent *)
{
	pm_settings_->exec(QCursor::pos());
}

void PsiChatDlgB::capsChanged()
{
	ChatDlg::capsChanged();

	QString resource = jid().resource();
	UserListItem *ul = account()->findFirstRelevant(jid());
	if (resource.isEmpty() && ul && !ul->userResourceList().isEmpty()) {
		resource = (*(ul->userResourceList().priority())).name();
	}
	act_voice_->setEnabled(!account()->capsManager()->isEnabled() || (ul && ul->isAvailable() && account()->capsManager()->features(jid().withResource(resource)).canVoice()));
}

void PsiChatDlgB::activated()
{
	ChatDlg::activated();

	updateCountVisibility();
}

void PsiChatDlgB::setContactToolTip(QString text)
{
	Q_UNUSED(text);
	//ui_.lb_status->setToolTip(text);
	//ui_.avatar->setToolTip(text);
}

void PsiChatDlgB::contactUpdated(UserListItem* u, int status, const QString& statusString)
{
	Q_UNUSED(statusString);

	Q_UNUSED(status);
	/*if (status == -1 || !u) {
		ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
	}
	else {
		ui_.lb_status->setPsiIcon(PsiIconset::instance()->statusPtr(jid(), status));
	}*/

	if (u) {
		setContactToolTip(u->makeTip(true, false));
	}
	else {
		setContactToolTip(QString());
	}

	if (u) {
		QString name;
		QString j;
		if (jid().resource().isEmpty())
			j = JIDUtil::toString(u->jid(), true);
		else
			j = JIDUtil::toString(u->jid().userHost(), false) + '/' + jid().resource();

		if (!u->name().isEmpty())
			name = u->name() + QString(" <%1>").arg(j);
		else
			name = j;

		ui_.le_jid->setText(name);
		ui_.le_jid->setCursorPosition(0);
		ui_.le_jid->setToolTip(name);
	}
}

void PsiChatDlgB::updateAvatar()
{
	QString res;
	QString client;

	if (!PsiOptions::instance()->getOption("options.ui.chat.avatars.show").toBool()) {
		//ui_.avatar->hide();
		return;
	}

	UserListItem *ul = account()->findFirstRelevant(jid());
	if (ul && !ul->userResourceList().isEmpty()) {
		UserResourceList::Iterator it = ul->userResourceList().find(jid().resource());
		if (it == ul->userResourceList().end())
			it = ul->userResourceList().priority();

		res = (*it).name();
		client = (*it).clientName();
	}
	//QPixmap p = account()->avatarFactory()->getAvatar(jid().withResource(res),client);
	QPixmap p = account()->avatarFactory()->getAvatar(jid().withResource(res));
	/*if (p.isNull()) {
		ui_.avatar->hide();
	}
	else {
		int size = PsiOptions::instance()->getOption("options.ui.chat.avatars.size").toInt();
		ui_.avatar->setPixmap(p.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		ui_.avatar->show();
	}*/

	QImage img;
	if(!p.isNull())
		img = p.toImage();
	lb_peerpic->setPixmap(QPixmap::fromImage(makeAvatarImage(img)));
}

void PsiChatDlgB::optionsUpdate()
{
	//smallChat_ = option.smallChats;

	ChatDlg::optionsUpdate();
}

void PsiChatDlgB::updatePGP()
{
	if (account()->hasPGP()) {
		act_pgp_->setEnabled(true);
	}
	else {
		act_pgp_->setChecked(false);
		act_pgp_->setEnabled(false);
	}

	ui_.tb_pgp->setVisible(account()->hasPGP() &&
	                       !smallChat_ &&
	                       !PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool());
}

void PsiChatDlgB::doClearButton()
{
	int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"), tr("&Yes"), tr("&No"));
	if (n == 0)
		doClear();
}

void PsiChatDlgB::setPGPEnabled(bool enabled)
{
	act_pgp_->setChecked(enabled);
}

void PsiChatDlgB::toggleSmallChat()
{
	smallChat_ = !smallChat_;
	setLooks();
}

void PsiChatDlgB::buildMenu()
{
	// Dialog menu
	pm_settings_->clear();
	//pm_settings_->addAction(act_compact_);
	pm_settings_->addAction(act_clear_);
	pm_settings_->insertSeparator();

	pm_settings_->addAction(act_icon_);
	pm_settings_->addAction(act_file_);
	if (account()->voiceCaller())
		act_voice_->addTo(pm_settings_);
	//pm_settings_->addAction(act_pgp_);
	pm_settings_->insertSeparator();

	pm_settings_->addAction(act_info_);
	pm_settings_->addAction(act_history_);
	pm_settings_->addAction(account()->psi()->tabChatsAction());
	pm_settings_->addAction(act_invite_);
}

void PsiChatDlgB::updateCounter()
{
	ui_.lb_count->setNum(chatEdit()->text().length());
}

void PsiChatDlgB::appendEmoteMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt)
{
	account()->extendDisclaimerTime(this->jid().bare());

	updateLastMsgTime(time);
	QString color = colorString(local, spooled);
	QString timestr = chatView()->formatTimeStamp(time);

	chatView()->appendText(QString("<span style=\"color: %1\">").arg(color) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(whoNick(local)) + txt + "</span>");
}

void PsiChatDlgB::appendNormalMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt)
{
	account()->extendDisclaimerTime(this->jid().bare());

	updateLastMsgTime(time);
	QString color = colorString(local, spooled);
	QString timestr = chatView()->formatTimeStamp(time);

	if (PsiOptions::instance()->getOption("options.ui.chat.use-chat-says-style").toBool()) {
		chatView()->appendText(QString("<p style=\"color: %1\">").arg(color) + QString("[%1] ").arg(timestr) + tr("%1 says:").arg(whoNick(local)) + "</p>" + txt);
	}
	else {
		chatView()->appendText(QString("<span style=\"color: %1\">").arg(color) + QString("[%1] &lt;").arg(timestr) + whoNick(local) + QString("&gt;</span> ") + txt);
	}
}

void PsiChatDlgB::appendMessageFields(const Message& m)
{
	if (!m.subject().isEmpty()) {
		chatView()->appendText(QString("<b>") + tr("Subject:") + "</b> " + QString("%1").arg(Qt::escape(m.subject())));
	}
	if (!m.urlList().isEmpty()) {
		UrlList urls = m.urlList();
		chatView()->appendText(QString("<i>") + tr("-- Attached URL(s) --") + "</i>");
		for (QList<Url>::ConstIterator it = urls.begin(); it != urls.end(); ++it) {
			const Url &u = *it;
			chatView()->appendText(QString("<b>") + tr("URL:") + "</b> " + QString("%1").arg(TextUtil::linkify(Qt::escape(u.url()))));
			chatView()->appendText(QString("<b>") + tr("Desc:") + "</b> " + QString("%1").arg(u.desc()));
		}
	}
}

bool PsiChatDlgB::isEncryptionEnabled() const
{
	return act_pgp_->isChecked();
}

void PsiChatDlgB::appendSysMsg(const QString &str)
{
	QDateTime t = QDateTime::currentDateTime();
	updateLastMsgTime(t);
	QString timestr = chatView()->formatTimeStamp(t);
	QString color = "#00A000";

	chatView()->appendText(QString("<font color=\"%1\">[%2]").arg(color, timestr) + QString(" *** %1</font>").arg(str));
}

QString PsiChatDlgB::colorString(bool local, ChatDlg::SpooledType spooled) const
{
	if (spooled == ChatDlg::Spooled_OfflineStorage)
		return "#008000";

	if (local)
		return "#FF0000";

	return "#0000FF";
}

ChatView* PsiChatDlgB::chatView() const
{
	return ui_.log;
}

ChatEdit* PsiChatDlgB::chatEdit() const
{
	return ui_.mle->chatEdit();
}

void PsiChatDlgB::chatEditCreated()
{
	ChatDlg::chatEditCreated();

	connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(updateCounter()));
	chatEdit()->installEventFilter(this);
}

void PsiChatDlgB::updateLastMsgTime(QDateTime t)
{
	bool doInsert = t.date() != lastMsgTime_.date();
	lastMsgTime_ = t;
	if (doInsert) {
		QString color = "#00A000";
		chatView()->appendText(QString("<font color=\"%1\">*** %2</font>").arg(color).arg(t.date().toString(Qt::ISODate)));
	}
}

// ###cuda
void PsiChatDlgB::doTabToggle()
{
	bool b = account()->psi()->isTabsModeEnabled();
	account()->psi()->setTabsModeEnabled(!b);
}

void PsiChatDlgB::reset_conversation_time()
{
	conversation_time_up = true;
}

void PsiChatDlgB::vcardChanged(const Jid &j)
{
	if(account()->jid().compare(j, false))
	{
		QImage img;
		QPixmap avatarPixmap = account()->avatarFactory()->getAvatar(account()->jid());
		if(!avatarPixmap.isNull())
			img = avatarPixmap.toImage();
		lb_selfpic->setPixmap(QPixmap::fromImage(makeAvatarImage(img)));
	}
}

void PsiChatDlgB::avatar_clicked()
{
	account()->changeVCard();
}

QString PsiChatDlgB::getDisclaimer(const Jid &jid)
{
	Q_UNUSED(jid);
	QString disclaimer;

	if(account()->needDisclaimer(this->jid().bare()))
		disclaimer = account()->disclaimer();

	return disclaimer;
}

QString PsiChatDlgB_getDisclaimer(ChatDlg *c, const Jid &jid)
{
	if(chat_dialogs()->contains(c))
	{
		PsiChatDlgB *pc = (PsiChatDlgB *)c;
		return pc->getDisclaimer(jid);
	}
	return QString();
}

void PsiChatDlgB::doInvite()
{
	if(invite_)
	{
		::bringToFront(invite_);
		return;
	}

	invite_ = new InviteDlg(false, jid(), account(), this);
	invite_->setAttribute(Qt::WA_DeleteOnClose);
	invite_->show();
}

bool PsiChatDlgB::readyToHide()
{
	delete invite_;
	return ChatDlg::readyToHide();
}
