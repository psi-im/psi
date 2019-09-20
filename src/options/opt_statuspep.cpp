#include "opt_statuspep.h"

#include "psioptions.h"
#include "ui_opt_statuspep.h"

#include <QCheckBox>
#include <QListWidget>

static const char *tuneUrlFilterOptionPath = "options.extended-presence.tune.url-filter";
static const char *tuneControllerFilterOptionPath = "options.extended-presence.tune.controller-filter";
static const char *tunePublishOptionPath = "options.extended-presence.tune.publish";

class OptStatusPepUI : public QWidget, public Ui::OptStatusPep
{
public:
    OptStatusPepUI() : QWidget() { setupUi(this); }
};

OptionsTabStatusPep::OptionsTabStatusPep(QObject *parent)
: OptionsTab(parent, "status_tunes", "", tr("PEP"), tr("Tunes no-video filter and controllers switcher"))
, w_(nullptr)
, psi_(nullptr)
, controllersChanged_(false)
{
}

OptionsTabStatusPep::~OptionsTabStatusPep()
{
}

QWidget *OptionsTabStatusPep::widget()
{
    if (w_) {
        return nullptr;
    }

    w_ = new OptStatusPepUI();
    OptStatusPepUI *d = static_cast<OptStatusPepUI *>(w_);
    connect(d->groupBox, &QGroupBox::toggled, this, [this](bool toggled){
        changeVisibleState(toggled);
        emit dataChanged();
    });

    return w_;
}

void OptionsTabStatusPep::applyOptions()
{
    if (!w_) {
        return;
    }

    OptStatusPepUI *d = static_cast<OptStatusPepUI *>(w_);
    PsiOptions* o = PsiOptions::instance();
    bool publishTune = d->groupBox->isChecked();
    o->setOption(tunePublishOptionPath, publishTune);
    if(publishTune) {
        QStringList newTuneFilters = d->tuneExtensions->text().split(QRegExp("\\W+"));
        QString tuneExstensionsFilter;
        if (!newTuneFilters.isEmpty()) {
            newTuneFilters.removeDuplicates();
            std::sort(newTuneFilters.begin(), newTuneFilters.end());
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
}

void OptionsTabStatusPep::restoreOptions()
{
    if (!w_) {
        return;
    }

    OptStatusPepUI *d = static_cast<OptStatusPepUI *>(w_);
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
        connect(cb, &QCheckBox::toggled, this, [this, cb](bool checked){
            QString name_ = cb->objectName();
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
        });
    }
    bool publishTune = o->getOption(tunePublishOptionPath).toBool();
    d->groupBox->setChecked(publishTune);
    changeVisibleState(publishTune);
}

void OptionsTabStatusPep::changeVisibleState(bool state)
{
    if (!w_) {
        return;
    }

    OptStatusPepUI *d = static_cast<OptStatusPepUI *>(w_);
    d->tuneExtensions->setVisible(state);
    d->rightGroupBox->setVisible(state);
    d->label->setVisible(state);
}

void OptionsTabStatusPep::setData(PsiCon *psi, QWidget *)
{
    psi_ = psi;
}
