#include "opt_appearance.h"
#include "opt_iconset.h"
#include "common.h"
#include "iconwidget.h"

#include <QWhatsThis>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPainter>
#include <QPixmap>
#include <QColorDialog>
#include <QFontDialog>
#include <QLineEdit>

#include "ui_opt_appearance.h"
#include "ui_opt_appearance_misc.h"
#include "psioptions.h"


class OptAppearanceUI : public QWidget, public Ui::OptAppearance
{
public:
	OptAppearanceUI() : QWidget() { setupUi(this); }
};

class OptAppearanceMiscUI : public QWidget, public Ui::OptAppearanceMisc
{
public:
	OptAppearanceMiscUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// FontLabel
//----------------------------------------------------------------------------

FontLabel::FontLabel(QWidget *parent, const char *name)
	: QLineEdit(parent)
{
	setObjectName(name);
	setReadOnly(true);

	QPalette palette = this->palette();
	palette.setColor(backgroundRole(), parent->palette().color(parent->backgroundRole()));
	setPalette(palette);

	m_defaultHeight = QLineEdit::sizeHint().height();
}

void FontLabel::setFont(QString fontName)
{
	QFont f;
	f.fromString(fontName);
	m_font = fontName;
	setText( tr("%1 %2").arg( f.family() ).arg( f.pointSize() ) );
	QLineEdit::setFont(f);
}

QString FontLabel::fontName() const
{
	return m_font;
}

QSize FontLabel::sizeHint() const
{
	return QSize(QLineEdit::sizeHint().width(), m_defaultHeight);
}


//----------------------------------------------------------------------------
// OptionsTabAppearance
//----------------------------------------------------------------------------
OptionsTabAppearance::OptionsTabAppearance(QObject *parent) : MetaOptionsTab(parent, "appearance", "", tr("Appearance"), tr("Psi's appearance"), "psi/appearance")
{
	addTab( new OptionsTabAppearanceGeneral(this) );
	addTab( new OptionsTabIconsetEmoticons(this) );
	addTab( new OptionsTabIconsetRoster(this) );
	addTab( new OptionsTabIconsetSystem(this) );
	addTab( new OptionsTabAppearanceMisc(this) );
}

//----------------------------------------------------------------------------
// OptionsTabAppearanceMisc
//----------------------------------------------------------------------------

OptionsTabAppearanceMisc::OptionsTabAppearanceMisc(QObject *parent)
	: OptionsTab(parent, "appearance_misc", "", tr("Misc."), tr("Miscellaneous Settings"))
	, w(0)
{
}

OptionsTabAppearanceMisc::~OptionsTabAppearanceMisc()
{
}

QWidget *OptionsTabAppearanceMisc::widget()
{
	if ( w )
		return 0;

	w = new OptAppearanceMiscUI();

	return w;
}

void OptionsTabAppearanceMisc::applyOptions()
{
	if ( !w )
		return;

	OptAppearanceMiscUI *d = (OptAppearanceMiscUI *)w;

	PsiOptions::instance()->setOption("options.ui.look.contactlist.use-slim-group-headings", d->ck_newHeadings->isChecked());
	PsiOptions::instance()->setOption("options.ui.look.contactlist.use-outlined-group-headings", d->ck_outlineHeadings->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.opacity", d->sl_rosterop->value());
	PsiOptions::instance()->setOption("options.ui.chat.opacity", d->sl_chatdlgop->value());
}

void OptionsTabAppearanceMisc::restoreOptions()
{
	if ( !w )
		return;

	OptAppearanceMiscUI *d = (OptAppearanceMiscUI *)w;

	d->ck_newHeadings->setChecked(PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool());
	d->ck_outlineHeadings->setChecked(PsiOptions::instance()->getOption("options.ui.look.contactlist.use-outlined-group-headings").toBool());
	
	d->sl_rosterop->setValue(PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt());
	d->sl_chatdlgop->setValue(PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt());
}

void OptionsTabAppearanceMisc::setData(PsiCon *, QWidget *parentDialog)
{
	parentWidget = parentDialog;
}

//----------------------------------------------------------------------------
// OptionsTabAppearanceGeneral: Fonts & Colours
//----------------------------------------------------------------------------

OptionsTabAppearanceGeneral::OptionsTabAppearanceGeneral(QObject *parent)
	: OptionsTab(parent, "appearance_general", "", tr("Fonts && Colors"), tr("Fonts && Color Settings"))
	, w(0)
	, bg_color(0)
	, bg_font(0)
{
}

OptionsTabAppearanceGeneral::~OptionsTabAppearanceGeneral()
{
	if ( bg_font )
		delete bg_font;
	if ( bg_color )
		delete bg_color;
}

static QPixmap color2pixmap(QColor c) // taken from opt_general.cpp
{
	QPixmap pix(16, 16);
	QPainter p(&pix);

	p.fillRect(0, 0, pix.width(), pix.height(), QBrush(c));
	p.setPen( QColor(0, 0, 0) );
	p.drawRect(0, 0, pix.width(), pix.height());
	p.end();

	return pix;
}

QWidget *OptionsTabAppearanceGeneral::widget()
{
	if ( w )
		return 0;

	w = new OptAppearanceUI();
	OptAppearanceUI *d = (OptAppearanceUI *)w;

	le_font[0] = d->le_fRoster;
	le_font[1] = d->le_fMessage;
	le_font[2] = d->le_fChat;
	le_font[3] = d->le_fPopup;

	bg_font = new QButtonGroup;
	bg_font->addButton(d->pb_fRoster);
	bg_font->addButton(d->pb_fMessage);
	bg_font->addButton(d->pb_fChat);
	bg_font->addButton(d->pb_fPopup);
	connect(bg_font, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(chooseFont(QAbstractButton*)));

	le_font[0]->setWhatsThis(
		tr("Specifies the font style for the main window."));
	le_font[1]->setWhatsThis(
		tr("Specifies the font style for message windows."));
	le_font[2]->setWhatsThis(
		tr("Specifies the font style for chat windows."));
	le_font[3]->setWhatsThis(
		tr("Specifies the font style for popup windows."));
	d->pb_fRoster->setWhatsThis(
		tr("Selects a font for the roster window using the font selection dialog."));
	d->pb_fMessage->setWhatsThis(
		tr("Selects a font for message windows using the font selection dialog."));
	d->pb_fChat->setWhatsThis(
		tr("Selects a font for chat windows using the font selection dialog."));

	bg_color = new QButtonGroup;
	bg_color->addButton(d->pb_cOnline);
	bg_color->addButton(d->pb_cOffline);
	bg_color->addButton(d->pb_cAway);
	bg_color->addButton(d->pb_cDND);
	bg_color->addButton(d->pb_cProfileFore);
	bg_color->addButton(d->pb_cProfileBack);
	bg_color->addButton(d->pb_cGroupFore);
	bg_color->addButton(d->pb_cGroupBack);
	bg_color->addButton(d->pb_cListBack);
	bg_color->addButton(d->pb_cAnimFront);
	bg_color->addButton(d->pb_cAnimBack);
	bg_color->addButton(d->pb_cStatus);
	bg_color->addButton(d->pb_cMessageSent);
	bg_color->addButton(d->pb_cMessageReceived);
	bg_color->addButton(d->pb_cSysMsg);
	connect(bg_color, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(chooseColor(QAbstractButton*)));

	QString s = tr("Specifies the text color for a contact name in the main window when that user is \"%1\".");
	d->pb_cOnline->setWhatsThis(
		s.arg(tr("online")));
	d->pb_cOffline->setWhatsThis(
		s.arg(tr("offline")));
	d->pb_cAway->setWhatsThis(
		s.arg(tr("away")));
	d->pb_cDND->setWhatsThis(
		s.arg(tr("do not disturb")));
	d->pb_cStatus->setWhatsThis(
		s.arg(tr("Status message")));
	d->pb_cProfileBack->setWhatsThis(
		tr("Specifies the background color for an account name in the main window."));
	d->pb_cGroupBack->setWhatsThis(
		tr("Specifies the background color for a group name in the main window."));
	d->pb_cListBack->setWhatsThis(
		tr("Specifies the background color for the main window."));
	d->pb_cAnimFront->setWhatsThis(
		tr("Specifies the foreground animation color for nicks."));
	d->pb_cAnimBack->setWhatsThis(
		tr("Specifies the background animation color for nicks."));
	d->pb_cMessageSent->setWhatsThis(
		tr("Specifies the color for sent messages in chat and history windows."));
	d->pb_cMessageReceived->setWhatsThis(
		tr("Specifies the color for received messages in chat and history windows."));
	d->pb_cSysMsg->setWhatsThis(
		tr("Specifies the color for informational messages in chat windows, like status changes and offline messages."));

	// Avatars
	//QWhatsThis::add(d->ck_avatarsChatdlg,
	//	tr("Toggles displaying of avatars in the chat dialog"));

	if (PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool()) {
		d->tl_cStatus->hide();
		d->pb_cStatus->hide();
	}
	
	return w;
}


static QColor getColor(QAbstractButton *button)
{
	return button->property("psi_color").value<QColor>();
}

void OptionsTabAppearanceGeneral::applyOptions()
{
	if ( !w )
		return;

	OptAppearanceUI *d = (OptAppearanceUI *)w;
	//LEGOPTS.avatarsChatdlgEnabled = d->ck_avatarsChatdlg->isChecked(); // Avatars

	PsiOptions::instance()->setOption("options.ui.look.font.contactlist", d->le_fRoster->fontName());
	PsiOptions::instance()->setOption("options.ui.look.font.message", d->le_fMessage->fontName());
	PsiOptions::instance()->setOption("options.ui.look.font.chat", d->le_fChat->fontName());
	PsiOptions::instance()->setOption("options.ui.look.font.passive-popup", d->le_fPopup->fontName());

	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.status.online", getColor(d->pb_cOnline));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.status.offline", getColor(d->pb_cOffline));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.status.away", getColor(d->pb_cAway));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.status.do-not-disturb", getColor(d->pb_cDND));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.profile.header-foreground", getColor(d->pb_cProfileFore));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.profile.header-background", getColor(d->pb_cProfileBack));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.grouping.header-foreground", getColor(d->pb_cGroupFore));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.grouping.header-background", getColor(d->pb_cGroupBack));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.background", getColor(d->pb_cListBack));
	PsiOptions::instance()->setOption("options.ui.look.contactlist.status-change-animation.color1", getColor(d->pb_cAnimFront));
	PsiOptions::instance()->setOption("options.ui.look.contactlist.status-change-animation.color2", getColor(d->pb_cAnimBack));
	PsiOptions::instance()->setOption("options.ui.look.colors.contactlist.status-messages", getColor(d->pb_cStatus));
	PsiOptions::instance()->setOption("options.ui.look.colors.messages.received", getColor(d->pb_cMessageReceived));
	PsiOptions::instance()->setOption("options.ui.look.colors.messages.sent", getColor(d->pb_cMessageSent));
	PsiOptions::instance()->setOption("options.ui.look.colors.messages.informational", getColor(d->pb_cSysMsg));
}

static void restoreColor(QToolButton *button, QColor c)
{
	button->setProperty("psi_color", c);
	button->setIcon(color2pixmap(c));
}

void OptionsTabAppearanceGeneral::restoreOptions()
{
	if ( !w )
		return;

	OptAppearanceUI *d = (OptAppearanceUI *)w;
	//d->ck_avatarsChatdlg->setChecked( LEGOPTS.avatarsChatdlgEnabled ); // Avatars

	d->le_fRoster->setFont(PsiOptions::instance()->getOption("options.ui.look.font.contactlist").toString());
	d->le_fMessage->setFont(PsiOptions::instance()->getOption("options.ui.look.font.message").toString());
	d->le_fChat->setFont(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());
	d->le_fPopup->setFont(PsiOptions::instance()->getOption("options.ui.look.font.passive-popup").toString());

	restoreColor(d->pb_cOnline, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.online").value<QColor>());
	restoreColor(d->pb_cOffline, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.offline").value<QColor>());
	restoreColor(d->pb_cAway, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.away").value<QColor>());
	restoreColor(d->pb_cDND, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.do-not-disturb").value<QColor>());
	restoreColor(d->pb_cProfileFore, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-foreground").value<QColor>());
	restoreColor(d->pb_cProfileBack, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-background").value<QColor>());
	restoreColor(d->pb_cGroupFore, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>());
	restoreColor(d->pb_cGroupBack, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
	restoreColor(d->pb_cListBack, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.background").value<QColor>());
	restoreColor(d->pb_cAnimFront, PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color1").value<QColor>());
	restoreColor(d->pb_cAnimBack, PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color2").value<QColor>());
	restoreColor(d->pb_cStatus, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status-messages").value<QColor>());
	restoreColor(d->pb_cMessageReceived, PsiOptions::instance()->getOption("options.ui.look.colors.messages.received").value<QColor>());
	restoreColor(d->pb_cMessageSent, PsiOptions::instance()->getOption("options.ui.look.colors.messages.sent").value<QColor>());
	restoreColor(d->pb_cSysMsg, PsiOptions::instance()->getOption("options.ui.look.colors.messages.informational").value<QColor>());
}

void OptionsTabAppearanceGeneral::setData(PsiCon *, QWidget *parentDialog)
{
	parentWidget = parentDialog;
}

void OptionsTabAppearanceGeneral::chooseFont(QAbstractButton* button)
{
	bool ok;
	QFont font;
	int x = (bg_font->buttons()).indexOf(button);
	font.fromString( le_font[x]->fontName() );

	// ensure we don't use the new native font dialog on mac with Qt 4.5,
	//   since it was broken last we checked (qt task #252000)
#if QT_VERSION >= 0x040500
	QString fnt = QFontDialog::getFont(&ok, font, parentWidget, QString(), QFontDialog::DontUseNativeDialog).toString();
#else
	QString fnt = QFontDialog::getFont(&ok, font, parentWidget).toString();
#endif
	le_font[x]->setFont(fnt);

	if(ok)
		emit dataChanged();
}

void OptionsTabAppearanceGeneral::chooseColor(QAbstractButton* button)
{
	QColor c;
	//int x = (bg_color->buttons()).indexOf(button);

	c = getColor(button);

	c = QColorDialog::getColor(c, parentWidget);
	if(c.isValid()) {
		button->setProperty("psi_color", c);
		//((QPushButton*) bg_color->buttons()[x])->setIcon(name2color(o->color[x].name()));
		button->setIcon(color2pixmap(c));

		emit dataChanged();
	}
}
