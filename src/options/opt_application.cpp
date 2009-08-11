#include "opt_application_b.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <q3groupbox.h>
#include <QList>
#include <QFontDialog>

#include "ui_opt_application.h"

class OptApplicationUI : public QWidget, public Ui::OptApplication
{
public:
	OptApplicationUI() : QWidget() { setupUi(this); }
};

class OptionsTabApplicationPrivate : public QObject
{
	Q_OBJECT
public:
	OptionsTabApplication *q;

	QWidget *w, *parentWidget;
	FontLabel *le_font[4];
	QButtonGroup *bg_font;

public slots:
	void chooseFont(QAbstractButton *button)
	{
		bool ok;
		QFont font;
		int x = (bg_font->buttons()).indexOf(button);
		font.fromString( le_font[x]->fontName() );

		QString fnt = QFontDialog::getFont(&ok, font, parentWidget).toString();
		if(ok)
		{
			le_font[x]->setFont(fnt);
			emit q->dataChanged();
		}
	}
};

//----------------------------------------------------------------------------
// OptionsTabApplication
//----------------------------------------------------------------------------

OptionsTabApplication::OptionsTabApplication(QObject *parent)
: OptionsTab(parent, "application", "", tr("Application"), tr("General application options"), "psi/logo_16")
{
	QWidget **pw = &w;
	OptionsTabApplicationPrivate **pp = (OptionsTabApplicationPrivate **)pw;
	*pp = new OptionsTabApplicationPrivate;
	OptionsTabApplicationPrivate * const p = (OptionsTabApplicationPrivate *)w;
	p->q = this;
	p->w = 0;
	p->bg_font = 0;
}

OptionsTabApplication::~OptionsTabApplication()
{
	OptionsTabApplicationPrivate * const p = (OptionsTabApplicationPrivate *)w;
	if ( p->bg_font )
		delete p->bg_font;
	delete p;
}

QWidget *OptionsTabApplication::widget()
{
	OptionsTabApplicationPrivate * const p = (OptionsTabApplicationPrivate *)w;

	if ( p->w )
		return 0;


	p->w = new OptApplicationUI();
	p->parentWidget = p->w; // ###cuda FIXME: wrong, but easy
	OptApplicationUI *d = (OptApplicationUI *)p->w;

	d->ck_alwaysOnTop->setWhatsThis(
		tr("Makes the main contacts window always be in front of other windows."));

	// docklet
	d->ck_docklet->setWhatsThis(
		tr("Enable the system tray icon, which allows quick access to some Barracuda functions, and also allows you to close the contacts window and leave the Barracuda client running."));
	d->ck_dockHideMW->setWhatsThis(
		tr("Starts the Barracuda client with only the system tray icon visible."));

#ifdef Q_WS_MAC
	d->ck_alwaysOnTop->hide();
	d->ck_docklet->hide();
#endif

	d->ck_scrollTo->setWhatsThis(
		tr("Makes the Barracuda client scroll the main window automatically so you can see new incoming events."));
	d->ck_ignoreHeadline->setWhatsThis(
		tr("Makes the Barracuda client ignore all incoming \"headline\" events,"
		" like system-wide news on MSN, announcements, etc."));

	p->le_font[0] = d->le_fChat;
	p->bg_font = new QButtonGroup;
	p->bg_font->insert(d->pb_fChat);
	connect(p->bg_font, SIGNAL(buttonClicked(QAbstractButton*)), p, SLOT(chooseFont(QAbstractButton*)));

	return p->w;
}

void OptionsTabApplication::applyOptions()
{
	OptionsTabApplicationPrivate * const p = (OptionsTabApplicationPrivate *)w;

	if ( !p->w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)p->w;

	PsiOptions::instance()->setOption("options.ui.contactlist.always-on-top", d->ck_alwaysOnTop->isChecked());

	// docklet
	PsiOptions::instance()->setOption("options.ui.systemtray.enable", d->ck_docklet->isChecked());
	PsiOptions::instance()->setOption("options.contactlist.hide-on-start", d->ck_dockHideMW->isChecked());

	PsiOptions::instance()->setOption("options.ui.contactlist.opacity", 100 - d->sl_rostertrans->value());
	PsiOptions::instance()->setOption("options.ui.chat.opacity", 100 - d->sl_chatdlgtrans->value());

	PsiOptions::instance()->setOption("options.ui.contactlist.ensure-contact-visible-on-event", d->ck_scrollTo->isChecked());
	PsiOptions::instance()->setOption("options.messages.ignore-headlines", d->ck_ignoreHeadline->isChecked());

	PsiOptions::instance()->setOption("options.ui.look.font.message", d->le_fChat->fontName());
	PsiOptions::instance()->setOption("options.ui.look.font.chat", d->le_fChat->fontName());

	// data transfer
	PsiOptions::instance()->setOption("options.p2p.bytestreams.listen-port", d->le_dtPort->text().toInt());
}

void OptionsTabApplication::restoreOptions()
{
	OptionsTabApplicationPrivate * const p = (OptionsTabApplicationPrivate *)w;

	if ( !p->w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)p->w;

	d->ck_alwaysOnTop->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool() );

	// docklet
	d->ck_docklet->setChecked( PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() );
	d->ck_dockHideMW->setChecked( PsiOptions::instance()->getOption("options.contactlist.hide-on-start").toBool() );

	d->sl_rostertrans->setValue( 100 - PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt() );
	d->sl_chatdlgtrans->setValue( 100 - PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt() );

	d->ck_scrollTo->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool() );
	d->ck_ignoreHeadline->setChecked( PsiOptions::instance()->getOption("options.messages.ignore-headlines").toBool() );

	d->le_fChat->setFont(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());

	// data transfer
	d->le_dtPort->setText( QString::number(PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt()) );
}

#include "opt_application.moc"
