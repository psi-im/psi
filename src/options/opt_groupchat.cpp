#include "opt_groupchat.h"
#include "common.h"
#include "iconwidget.h"

#include <qbuttongroup.h>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qsignalmapper.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qcolordialog.h>

#include "ui_opt_general_groupchat.h"

class GeneralGroupchatUI : public QWidget, public Ui::GeneralGroupchat
{
public:
	GeneralGroupchatUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabGroupchat -- TODO: simplify the code
//----------------------------------------------------------------------------

OptionsTabGroupchat::OptionsTabGroupchat(QObject *parent)
: OptionsTab(parent, "groupchat", "", tr("Groupchat"), tr("Configure the groupchat"), "psi/groupChat")
{
	w = 0;
}

void OptionsTabGroupchat::setData(PsiCon *, QWidget *_dlg)
{
	dlg = _dlg;
}

QWidget *OptionsTabGroupchat::widget()
{
	if ( w )
		return 0;

	w = new GeneralGroupchatUI();
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;

	connect(d->pb_nickColor,	   SIGNAL(clicked()), SLOT(chooseGCNickColor()));
	connect(d->lb_nickColors,	   SIGNAL(highlighted(int)), SLOT(selectedGCNickColor()));

	connect(d->pb_addHighlightWord,	   SIGNAL(clicked()), SLOT(addGCHighlight()));
	connect(d->pb_removeHighlightWord, SIGNAL(clicked()), SLOT(removeGCHighlight()));

	connect(d->pb_addNickColor,	   SIGNAL(clicked()), SLOT(addGCNickColor()));
	connect(d->pb_removeNickColor,	   SIGNAL(clicked()), SLOT(removeGCNickColor()));

	// TODO: add QWhatsThis for all controls on widget

	return w;
}

void OptionsTabGroupchat::applyOptions(Options *opt)
{
	if ( !w )
		return;

	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	opt->gcHighlighting = d->ck_gcHighlights->isChecked();
	opt->gcNickColoring = d->ck_gcNickColoring->isChecked();

	QStringList highlight;
	int i;
	for (i = 0; i < (int)d->lb_highlightWords->count(); i++)
		highlight << d->lb_highlightWords->item(i)->text();
	opt->gcHighlights = highlight;

	QStringList colors;
	for (i = 0; i < (int)d->lb_nickColors->count(); i++)
		colors << d->lb_nickColors->item(i)->text();
	opt->gcNickColors = colors;
}

void OptionsTabGroupchat::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;

	// no need to call dataChanged() when these widgets are modified
	disconnect(d->le_newNickColor,     SIGNAL(textChanged(const QString &)), 0, 0);
	disconnect(d->le_newHighlightWord, SIGNAL(textChanged(const QString &)), 0, 0);
	connect(d->le_newNickColor,	   SIGNAL(textChanged(const QString &)), SLOT(displayGCNickColor()));

	d->ck_gcHighlights->setChecked( true );
	d->ck_gcHighlights->setChecked( opt->gcHighlighting );
	d->ck_gcNickColoring->setChecked( true );
	d->ck_gcNickColoring->setChecked( opt->gcNickColoring );
	d->lb_highlightWords->clear();
	d->lb_highlightWords->insertStringList( opt->gcHighlights );
	d->lb_nickColors->clear();

	QStringList::ConstIterator it = opt->gcNickColors.begin();
	for ( ; it != opt->gcNickColors.end(); ++it)
		addNickColor( *it );

	d->le_newHighlightWord->setText("");
	d->le_newNickColor->setText("#FFFFFF");
}

static QPixmap name2color(QString name)
{
	QColor c(name);
	QPixmap pix(16, 16);
	QPainter p(&pix);

	p.fillRect(0, 0, pix.width(), pix.height(), QBrush(c));
	p.setPen( QColor(0, 0, 0) );
	p.drawRect(0, 0, pix.width(), pix.height());
	p.end();

	return pix;
}

void OptionsTabGroupchat::addNickColor(QString name)
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	d->lb_nickColors->insertItem(name2color(name), name);
}

void OptionsTabGroupchat::addGCHighlight()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	if ( d->le_newHighlightWord->text().isEmpty() )
		return;

	d->lb_highlightWords->insertItem( d->le_newHighlightWord->text() );
	d->le_newHighlightWord->setFocus();
	d->le_newHighlightWord->setText("");

	emit dataChanged();
}

void OptionsTabGroupchat::removeGCHighlight()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	int id = d->lb_highlightWords->currentItem();
	if ( id == -1 )
		return;

	d->lb_highlightWords->removeItem(id);

	emit dataChanged();
}

void OptionsTabGroupchat::addGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	if ( d->le_newNickColor->text().isEmpty() )
		return;

	addNickColor( d->le_newNickColor->text() );
	d->le_newNickColor->setFocus();
	d->le_newNickColor->setText("");

	emit dataChanged();
}

void OptionsTabGroupchat::removeGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	int id = d->lb_nickColors->currentItem();
	if ( id == -1 )
		return;

	d->lb_nickColors->removeItem(id);

	emit dataChanged();
}

void OptionsTabGroupchat::chooseGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	QColor c = QColorDialog::getColor(QColor(d->le_newNickColor->text()), dlg);
	if ( c.isValid() ) {
		QString cs = c.name();
		d->le_newNickColor->setText(cs);
	}
}

void OptionsTabGroupchat::selectedGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	int id = d->lb_nickColors->currentItem();
	if ( id == -1 )
		return;

	d->le_newNickColor->setText( d->lb_nickColors->text(id) );
}

void OptionsTabGroupchat::displayGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	d->pb_nickColor->setPixmap( name2color(d->le_newNickColor->text()) );
}
