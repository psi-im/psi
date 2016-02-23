#include "opt_groupchat.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"

#include <QButtonGroup>
#include <QWhatsThis>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalMapper>
#include <QPixmap>
#include <QPainter>
#include <QColorDialog>

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
	connect(d->lw_nickColors,	   SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), SLOT(selectedGCNickColor(QListWidgetItem *)));

	connect(d->pb_addHighlightWord,	   SIGNAL(clicked()), SLOT(addGCHighlight()));
	connect(d->pb_removeHighlightWord, SIGNAL(clicked()), SLOT(removeGCHighlight()));

	connect(d->pb_addNickColor,	   SIGNAL(clicked()), SLOT(addGCNickColor()));
	connect(d->pb_removeNickColor,	   SIGNAL(clicked()), SLOT(removeGCNickColor()));

	connect(d->ck_gcHashNickColoring, SIGNAL(toggled(bool)), SLOT(updateWidgetsState()));
	connect(d->ck_gcNickColoring, SIGNAL(toggled(bool)), SLOT(updateWidgetsState()));
	connect(d->ck_gcHighlights, SIGNAL(toggled(bool)), SLOT(updateWidgetsState()));

	// TODO: add QWhatsThis for all controls on widget

	return w;
}

void OptionsTabGroupchat::applyOptions()
{
	if ( !w )
		return;

	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	PsiOptions::instance()->setOption("options.ui.muc.use-highlighting", d->ck_gcHighlights->isChecked());
	PsiOptions::instance()->setOption("options.ui.muc.use-nick-coloring", d->ck_gcNickColoring->isChecked());
	PsiOptions::instance()->setOption("options.ui.muc.use-hash-nick-coloring", d->ck_gcHashNickColoring->isChecked());

	QStringList highlight;
	int i;
	for (i = 0; i < (int)d->lw_highlightWords->count(); i++)
		highlight << d->lw_highlightWords->item(i)->text();
	PsiOptions::instance()->setOption("options.ui.muc.highlight-words", highlight);

	QStringList colors;
	for (i = 0; i < (int)d->lw_nickColors->count(); i++)
		colors << d->lw_nickColors->item(i)->text();
	PsiOptions::instance()->setOption("options.ui.look.colors.muc.nick-colors", colors);
}

void OptionsTabGroupchat::restoreOptions()
{
	if ( !w )
		return;

	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;

	// no need to call dataChanged() when these widgets are modified
	disconnect(d->le_newNickColor,     SIGNAL(textChanged(const QString &)), 0, 0);
	disconnect(d->le_newHighlightWord, SIGNAL(textChanged(const QString &)), 0, 0);
	connect(d->le_newNickColor,	   SIGNAL(textChanged(const QString &)), SLOT(displayGCNickColor()));

	d->ck_gcHighlights->setChecked( true );
	d->ck_gcHighlights->setChecked( PsiOptions::instance()->getOption("options.ui.muc.use-highlighting").toBool() );
	d->ck_gcNickColoring->setChecked( true );
	d->ck_gcNickColoring->setChecked( PsiOptions::instance()->getOption("options.ui.muc.use-nick-coloring").toBool() );
	d->ck_gcHashNickColoring->setChecked( true );
	d->ck_gcHashNickColoring->setChecked( PsiOptions::instance()->getOption("options.ui.muc.use-hash-nick-coloring").toBool() );
	d->lw_highlightWords->clear();
	d->lw_highlightWords->addItems( PsiOptions::instance()->getOption("options.ui.muc.highlight-words").toStringList() );
	d->lw_nickColors->clear();


	foreach(QString col, PsiOptions::instance()->getOption("options.ui.look.colors.muc.nick-colors").toStringList()) {
		addNickColor(col);
	}

	d->le_newHighlightWord->setText("");
	d->le_newNickColor->setText("#FFFFFF");
}

void OptionsTabGroupchat::updateWidgetsState()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;

	{
		bool enableHighlights = d->ck_gcHighlights->isChecked();
		d->lw_highlightWords->setEnabled(enableHighlights);
		d->le_newHighlightWord->setEnabled(enableHighlights);
		d->pb_addHighlightWord->setEnabled(enableHighlights);
		d->pb_removeHighlightWord->setEnabled(enableHighlights);
	}

	{
		bool enableNickColors = d->ck_gcNickColoring->isChecked() && !d->ck_gcHashNickColoring->isChecked();
		d->ck_gcNickColoring->setEnabled(!d->ck_gcHashNickColoring->isChecked());
		d->lw_nickColors->setEnabled(enableNickColors);
		d->le_newNickColor->setEnabled(enableNickColors);
		d->pb_addNickColor->setEnabled(enableNickColors);
		d->pb_removeNickColor->setEnabled(enableNickColors);
		d->pb_nickColor->setEnabled(enableNickColors);
	}
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
	d->lw_nickColors->addItem(new QListWidgetItem(name2color(name), name));
}

void OptionsTabGroupchat::addGCHighlight()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	if ( d->le_newHighlightWord->text().isEmpty() )
		return;

	d->lw_highlightWords->addItem( d->le_newHighlightWord->text() );
	d->le_newHighlightWord->setFocus();
	d->le_newHighlightWord->setText("");

	emit dataChanged();
}

void OptionsTabGroupchat::removeGCHighlight()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	int id = d->lw_highlightWords->currentRow();
	if ( id == -1 )
		return;

	d->lw_highlightWords->takeItem(id);

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
	int id = d->lw_nickColors->currentRow();
	if ( id == -1 )
		return;

	d->lw_nickColors->takeItem(id);

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

void OptionsTabGroupchat::selectedGCNickColor(QListWidgetItem * item)
{
	if (!item) return; // no selection on empty list
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	d->le_newNickColor->setText( item->text() );
}

void OptionsTabGroupchat::displayGCNickColor()
{
	GeneralGroupchatUI *d = (GeneralGroupchatUI *)w;
	d->pb_nickColor->setIcon( name2color(d->le_newNickColor->text()) );
}
