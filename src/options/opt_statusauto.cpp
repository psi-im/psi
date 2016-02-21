#include "opt_statusauto.h"
#include "psioptions.h"
#include "priorityvalidator.h"

#include <limits.h>
#include <QWhatsThis>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>

#include "ui_opt_statusauto.h"

class OptStatusAutoUI : public QWidget, public Ui::OptStatusAuto
{
public:
	OptStatusAutoUI() : QWidget() { setupUi(this); }
};

OptionsTabStatusAuto::OptionsTabStatusAuto(QObject *parent)
	: OptionsTab(parent, "status_auto", "", tr("Auto status and priority"), tr("Auto status and priority preferences"))
	, w(0)
{
}

OptionsTabStatusAuto::~OptionsTabStatusAuto()
{
}

QWidget *OptionsTabStatusAuto::widget()
{
	if ( w )
		return 0;

	w = new OptStatusAutoUI();
	OptStatusAutoUI *d = (OptStatusAutoUI *)w;

	PriorityValidator* prValidator = new PriorityValidator(d->le_asPriority);
	d->le_asPriority->setValidator(prValidator);
	prValidator = new PriorityValidator(d->le_asXaPriority);
	d->le_asXaPriority->setValidator(prValidator);

	QString s = tr("Makes Psi automatically set your status to \"away\" if your"
		" computer is idle for the specified amount of time.");
	d->ck_asAway->setWhatsThis(s);
	d->sb_asAway->setWhatsThis(s);
	s = tr("Makes Psi automatically set your status to \"extended away\" if your"
		" computer is idle for the specified amount of time.");
	d->ck_asXa->setWhatsThis(s);
	d->sb_asXa->setWhatsThis(s);
	s = tr("Makes Psi automatically set your status to \"offline\" if your"
		" computer is idle for the specified amount of time."
		"  This will disconnect you from the Jabber server.");
	PsiOptions* o = PsiOptions::instance();
	int dpCount = 6;
	if (!o->getOption("options.ui.menu.status.chat").toBool()) {
		d->sb_dpChat->hide();
		d->lb_dpChat->hide();
		dpCount--;
	}

	if (!o->getOption("options.ui.menu.status.xa").toBool()) {
		d->ck_asXa->hide();
		d->sb_asXa->hide();
		d->lb_asXaPriority->hide();
		d->le_asXaPriority->hide();
		d->sb_dpXa->hide();
		d->lb_dpXa->hide();
		dpCount--;
	}
	if (!o->getOption("options.ui.menu.status.invisible").toBool()) {
		d->sb_dpInvisible->hide();
		d->lb_dpInvisible->hide();
		dpCount--;
	}

	if (dpCount != 6) {
		reorderGridLayout(d->gridLayout, dpCount == 4 ? 2 : 3); //4 items in 2 columns look better
	}

	d->ck_asOffline->setWhatsThis( s);
	d->sb_asOffline->setWhatsThis( s);

	d->te_asMessage->setWhatsThis(
		tr("Specifies an extended message to use if you allow Psi"
		" to set your status automatically.  See options above."));
	d->le_asPriority->setWhatsThis(
		tr("Specifies priority of auto-away status. "
		"If empty, Psi will use account's default priority."));

	return w;
}

void OptionsTabStatusAuto::applyOptions()
{
	if ( !w )
		return;

	OptStatusAutoUI *d = (OptStatusAutoUI *)w;
	PsiOptions* o = PsiOptions::instance();

	o->setOption("options.status.auto-away.away-after", d->sb_asAway->value());
	o->setOption("options.status.auto-away.not-availible-after", d->sb_asXa->value());
	o->setOption("options.status.auto-away.offline-after", d->sb_asOffline->value());
	o->setOption("options.status.auto-away.use-away", d->ck_asAway->isChecked());
	o->setOption("options.status.auto-away.use-not-availible", d->ck_asXa->isChecked());
	o->setOption("options.status.auto-away.use-offline", d->ck_asOffline->isChecked());
	o->setOption("options.status.auto-away.message", d->te_asMessage->toPlainText());
	bool forcePriority = false;
	o->setOption("options.status.auto-away.priority", d->le_asPriority->text().toInt(&forcePriority));
	o->setOption("options.status.auto-away.force-priority", forcePriority);
	forcePriority = false;
	o->setOption("options.status.auto-away.xa-priority", d->le_asXaPriority->text().toInt(&forcePriority));
	o->setOption("options.status.auto-away.force-xa-priority", forcePriority);

	o->setOption("options.status.default-priority.online", d->sb_dpOnline->value());
	o->setOption("options.status.default-priority.chat", d->sb_dpChat->value());
	o->setOption("options.status.default-priority.away", d->sb_dpAway->value());
	o->setOption("options.status.default-priority.xa", d->sb_dpXa->value());
	o->setOption("options.status.default-priority.dnd", d->sb_dpDnd->value());
	o->setOption("options.status.default-priority.invisible", d->sb_dpInvisible->value());
}

void OptionsTabStatusAuto::restoreOptions()
{
	if ( !w )
		return;

	OptStatusAutoUI *d = (OptStatusAutoUI *)w;
	PsiOptions* o = PsiOptions::instance();

	d->sb_asAway->setMinimum(0);
	d->sb_asAway->setMaximum(INT_MAX);
	d->sb_asAway->setValue( o->getOption("options.status.auto-away.away-after").toInt() );
	d->sb_asXa->setMinimum(0);
	d->sb_asXa->setMaximum(INT_MAX);
	d->sb_asXa->setValue( o->getOption("options.status.auto-away.not-availible-after").toInt() );
	d->sb_asOffline->setMinimum(0);
	d->sb_asOffline->setMaximum(INT_MAX);
	d->sb_asOffline->setValue( o->getOption("options.status.auto-away.offline-after").toInt() );
	d->ck_asAway->setChecked( o->getOption("options.status.auto-away.use-away").toBool() );
	d->ck_asXa->setChecked( o->getOption("options.status.auto-away.use-not-availible").toBool() );
	d->ck_asOffline->setChecked( o->getOption("options.status.auto-away.use-offline").toBool() );
	d->te_asMessage->setPlainText( o->getOption("options.status.auto-away.message").toString() );
	if (o->getOption("options.status.auto-away.force-priority").toBool()) {
		d->le_asPriority->setText(QString::number(o->getOption("options.status.auto-away.priority").toInt()));
	} else {
		d->le_asPriority->clear();
	}
	if (o->getOption("options.status.auto-away.force-xa-priority").toBool()) {
		d->le_asXaPriority->setText(QString::number(o->getOption("options.status.auto-away.xa-priority").toInt()));
	} else {
		d->le_asXaPriority->clear();
	}

	d->sb_dpOnline->setValue(o->getOption("options.status.default-priority.online").toInt());
	d->sb_dpChat->setValue(o->getOption("options.status.default-priority.chat").toInt());
	d->sb_dpAway->setValue(o->getOption("options.status.default-priority.away").toInt());
	d->sb_dpXa->setValue(o->getOption("options.status.default-priority.xa").toInt());
	d->sb_dpDnd->setValue(o->getOption("options.status.default-priority.dnd").toInt());
	d->sb_dpInvisible->setValue(o->getOption("options.status.default-priority.invisible").toInt());
}

void OptionsTabStatusAuto::setData(PsiCon *, QWidget *parentDialog)
{
	parentWidget = parentDialog;
}

