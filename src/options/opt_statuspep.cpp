#include "opt_statuspep.h"
#include "psioptions.h"

#include <QListWidget>
#include <QCheckBox>

#include "ui_opt_statuspep.h"

static const char *tuneUrlFilterOptionPath = "options.extended-presence.tune.url-filter";
static const char *tuneControllerFilterOptionPath = "options.extended-presence.tune.controller-filter";

class OptStatusPepUI : public QWidget, public Ui::OptStatusPep
{
public:
	OptStatusPepUI() : QWidget() { setupUi(this); }
};

OptionsTabStatusPep::OptionsTabStatusPep(QObject *parent)
: OptionsTab(parent, "status_tunes", "", tr("PEP"), tr("Tunes no-video filter and controllers switcher"))
, w_(0)
, psi_(0)
, controllersChanged_(false)
{
}

OptionsTabStatusPep::~OptionsTabStatusPep()
{
}

QWidget *OptionsTabStatusPep::widget()
{
	if (w_) {
		return 0;
	}

	w_ = new OptStatusPepUI();
	return w_;
}

void OptionsTabStatusPep::applyOptions()
{
	if (!w_) {
		return;
	}

	OptStatusPepUI *d = (OptStatusPepUI *)w_;
	PsiOptions* o = PsiOptions::instance();
	QStringList newTuneFilters = d->tuneExtensions->text().split(QRegExp("\\W+"));
	QString tuneExstensionsFilter;
	if (!newTuneFilters.isEmpty()) {
		newTuneFilters.removeDuplicates();
		qSort(newTuneFilters);
		tuneExstensionsFilter = newTuneFilters.join(" ").toLower();
		d->tuneExtensions->setText(tuneExstensionsFilter);
	}
	QString controllerFilter = blackList_.join(",");
	if (tuneExstensionsFilter != tuneFilters_) {
		o->setOption(tuneUrlFilterOptionPath, tuneExstensionsFilter);
	}
	if (controllersChanged_) {
		o->setOption(tuneControllerFilterOptionPath, controllerFilter);
	}
}

void OptionsTabStatusPep::restoreOptions()
{
	if (!w_) {
		return;
	}

	OptStatusPepUI *d = (OptStatusPepUI *)w_;
	PsiOptions* o = PsiOptions::instance();
	tuneFilters_ = o->getOption(tuneUrlFilterOptionPath).toString();
	d->tuneExtensions->setText(tuneFilters_);
	QStringList controllers = psi_->tuneManager()->controllerNames();
	blackList_ = o->getOption(tuneControllerFilterOptionPath).toString().split(QRegExp("[,]\\s*"), QString::SkipEmptyParts);
	foreach(const QString &name, controllers) {
		QCheckBox* cb = new QCheckBox(name);
		QString caption = name + " controller";
		cb->setText(caption);
		cb->setObjectName(name);
		int i = controllers.indexOf(name);
		d->gridLayout->addWidget(cb,i/3,i%3);
		cb->setChecked(!blackList_.contains(name));
		connect(cb, SIGNAL(toggled(bool)), SLOT(controllerSelected(bool)));
	}
}

void OptionsTabStatusPep::controllerSelected(bool checked)
{
	QCheckBox *box = qobject_cast<QCheckBox*>(sender());
	QString name_ = box->objectName();
	if (!name_.isEmpty()) {
		if (!checked && !blackList_.contains(name_, Qt::CaseInsensitive)) {
			blackList_ << name_;
			controllersChanged_ = true;
		}
		else if (checked) {
			blackList_.removeAll(name_);
			controllersChanged_ = true;
		}
	}
}

void OptionsTabStatusPep::setData(PsiCon *psi, QWidget *)
{
	psi_ = psi;
}
