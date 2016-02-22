#include "opt_appearance.h"
#include "opt_iconset.h"
#include "opt_theme.h"
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
#include <QSignalMapper>

#include "ui_opt_appearance.h"
#include "ui_opt_appearance_misc.h"
#include "psioptions.h"
#include "coloropt.h"
#include "psithememanager.h"


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
	addTab( new OptionsTabIconset(this) );
	if (PsiThemeManager::instance()->registeredProviders().count()) {
		addTab( new OptionsTabAppearanceThemes(this) );
	}
	addTab( new OptionsTabAppearanceMisc(this) );
}


//----------------------------------------------------------------------------
// OptionsTabIconset
//----------------------------------------------------------------------------
OptionsTabIconset::OptionsTabIconset(QObject *parent) : MetaOptionsTab(parent, "iconsets", "", tr("Icons"), tr("Icons"))
{
	addTab( new OptionsTabIconsetEmoticons(this) );
	addTab( new OptionsTabIconsetMoods(this) );
	addTab( new OptionsTabIconsetActivity(this) );
	addTab( new OptionsTabIconsetClients(this) );
	addTab( new OptionsTabIconsetRoster(this) );
	addTab( new OptionsTabIconsetSystem(this) );
	addTab( new OptionsTabIconsetAffiliations(this) );
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



	QString s = tr("Specifies the text color for a contact name in the main window when that user is \"%1\".");
	struct ColorWidgetData {
		QCheckBox *cbox;
		QToolButton *button;
		QString option;
		QString descr;
	};
	ColorWidgetData cwData[] = {
		{d->ck_cOnline,  d->pb_cOnline,  "contactlist.status.online", s.arg(tr("online")) },
		{d->ck_cOffline, d->pb_cOffline, "contactlist.status.offline", s.arg(tr("offline")) },
		{d->ck_cAway,    d->pb_cAway,    "contactlist.status.away", s.arg(tr("away")) },
		{d->ck_cDND,     d->pb_cDND,     "contactlist.status.do-not-disturb", s.arg(tr("do not disturb")) },
		{d->ck_cStatus,  d->pb_cStatus,  "contactlist.status-messages", s.arg(tr("Status message"))},
		{d->ck_cProfileFore,     d->pb_cProfileFore, "contactlist.profile.header-foreground", ""},
		{d->ck_cProfileBack,     d->pb_cProfileBack, "contactlist.profile.header-background", ""},
		{d->ck_cGroupFore,       d->pb_cGroupFore,   "contactlist.grouping.header-foreground", ""},
		{d->ck_cGroupBack,       d->pb_cGroupBack,   "contactlist.grouping.header-background", ""},
		{d->ck_cListBack,        d->pb_cListBack,    "contactlist.background", ""},
		{d->ck_cAnimFront,       d->pb_cAnimFront,   "contactlist.status-change-animation1", ""},
		{d->ck_cAnimBack,        d->pb_cAnimBack,    "contactlist.status-change-animation2", ""},
		{d->ck_cMessageSent,     d->pb_cMessageSent,     "messages.sent", ""},
		{d->ck_cMessageReceived, d->pb_cMessageReceived, "messages.received", ""},
		{d->ck_cSysMsg,          d->pb_cSysMsg,          "messages.informational", ""},
		{d->ck_cUserText,        d->pb_cUserText,        "messages.usertext", ""},
		{d->ck_highlight,        d->pb_highlight,        "messages.highlighting", ""}
	};

	bg_color = new QButtonGroup(this);
	for (unsigned int i = 0; i < sizeof(cwData) / sizeof(ColorWidgetData); i++) {
		bg_color->addButton(cwData[i].button);
		if (!cwData[i].descr.isEmpty()) {
			cwData[i].cbox->setWhatsThis(cwData[i].descr);
		}
		connect(cwData[i].cbox, SIGNAL(stateChanged(int)), SLOT(colorCheckBoxClicked(int)));
		colorWidgetsMap[cwData[i].cbox] = QPair<QAbstractButton*,QString>(cwData[i].button, cwData[i].option);
	}
	connect(bg_color, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(chooseColor(QAbstractButton*)));

	// Avatars
	//QWhatsThis::add(d->ck_avatarsChatdlg,
	//	tr("Toggles displaying of avatars in the chat dialog"));

	if (PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool()) {
		d->ck_cStatus->hide();
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

	ColorWidgetsMap::ConstIterator i = colorWidgetsMap.constBegin();
	while (i != colorWidgetsMap.constEnd()) {
		PsiOptions::instance()->setOption("options.ui.look.colors." + i.value().second,
										  i.key()->isChecked()? getColor((QToolButton*)i.value().first) : QColor());
		++i;
	}
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

	ColorWidgetsMap::ConstIterator i = colorWidgetsMap.constBegin();
	while (i != colorWidgetsMap.constEnd()) {
		QColor color = ColorOpt::instance()->color("options.ui.look.colors." + i.value().second);
		QColor realColor = PsiOptions::instance()->getOption("options.ui.look.colors." + i.value().second).value<QColor>();
		i.key()->setChecked(realColor.isValid());
		restoreColor((QToolButton*)i.value().first, color);
		++i;
	}
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
	QString fnt = QFontDialog::getFont(&ok, font, parentWidget, QString(), QFontDialog::DontUseNativeDialog).toString();
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

void OptionsTabAppearanceGeneral::colorCheckBoxClicked(int state)
{
	QPair<QAbstractButton*,QString> data = colorWidgetsMap[(QCheckBox*)sender()];
	if (state) {
		data.first->setDisabled(false);
		restoreColor((QToolButton*)data.first, ColorOpt::instance()->color("options.ui.look.colors." + data.second));
	}
	else {
		//data.first->setDisabled(true); // TODO disable color changing
		QPalette::ColorRole role = ColorOpt::instance()->colorRole(
					"options.ui.look.colors." + data.second);
		data.first->setIcon(color2pixmap(QApplication::palette().color(role)));
	}
}
