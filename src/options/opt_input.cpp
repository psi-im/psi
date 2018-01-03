#include "opt_input.h"

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QCheckBox>
#include <QLocale>

#include "psioptions.h"
#include "spellchecker/spellchecker.h"


#include "ui_opt_input.h"

static const QString ENABLED_OPTION = QStringLiteral("options.ui.spell-check.enabled");
static const QString DICTS_OPTION = QStringLiteral("options.ui.spell-check.langs");
static const QString AUTORESIZE_OPTION = QStringLiteral("options.ui.chat.use-expanding-line-edit");
static const uint FullName = 0;


class OptInputUI : public QWidget, public Ui::OptInput
{
public:
    OptInputUI() : QWidget() { setupUi(this); }
};

OptionsTabInput::OptionsTabInput(QObject *parent)
: OptionsTab(parent, "input", "", tr("Input"), tr("Input options"), "psi/action_templates_edit"),
  w_(0),
  psi_(0)
{
}

OptionsTabInput::~OptionsTabInput()
{}

QWidget *OptionsTabInput::widget()
{
    if (w_) {
        return 0;
    }

    w_ = new OptInputUI();
    OptInputUI *d = (OptInputUI *)w_;

    availableDicts_ = SpellChecker::instance()->getAllLanguages();
    defaultLangs_ = LanguageManager::bestUiMatch(availableDicts_).toSet();

    d->isSpellCheck->setWhatsThis(tr("Check this option if you want your spelling to be checked"));

    connect(d->isSpellCheck, &QCheckBox::toggled, this, &OptionsTabInput::itemToggled);

    d->ck_autoCapitalize->setWhatsThis(
        tr("Enables automatic substitution of the first letter in a sentence to the same capital letter"));

    return w_;
}

void OptionsTabInput::applyOptions()
{
    if (!w_) {
        return;
    }

    OptInputUI *d = (OptInputUI *)w_;
    PsiOptions* o = PsiOptions::instance();
    SpellChecker *s = SpellChecker::instance();

    bool isEnabled = d->isSpellCheck->isChecked();
    o->setOption(ENABLED_OPTION, isEnabled);
    o->setOption(AUTORESIZE_OPTION, d->isAutoResize->isChecked());
    if(!isEnabled) {
        loadedDicts_.clear();
        s->setActiveLanguages(loadedDicts_);
    }
    else {
        d->groupBoxDicts->setEnabled(isEnabled);
        s->setActiveLanguages(loadedDicts_);
        QStringList loaded;
        for (auto const &id: loadedDicts_) {
            loaded.append(LanguageManager::toString(id));
        }
        o->setOption(DICTS_OPTION, QVariant(loaded.join(" ")));
    }
    o->setOption("options.ui.chat.auto-capitalize", d->ck_autoCapitalize->isChecked());
}

void OptionsTabInput::restoreOptions()
{
    if (!w_) {
        return;
    }

    OptInputUI *d = (OptInputUI *)w_;
    PsiOptions* o = PsiOptions::instance();

    updateDictLists();

    d->isAutoResize->setChecked( o->getOption(AUTORESIZE_OPTION).toBool() );
    bool isEnabled = o->getOption(ENABLED_OPTION).toBool();
    isEnabled = (!SpellChecker::instance()->available()) ? false : isEnabled;
    d->groupBoxDicts->setEnabled(isEnabled);
    d->isSpellCheck->setChecked(isEnabled);
    if (!availableDicts_.isEmpty()) {
        d->dictsWarnLabel->setVisible(false);
        if(isEnabled && isTreeViewEmpty()) {
            fillList();
        }
        if (isEnabled) {
            setChecked();
        }
    }
    else {
        d->dictsWarnLabel->setVisible(true);
    }
    d->ck_autoCapitalize->setChecked(o->getOption("options.ui.chat.auto-capitalize").toBool());
}

void OptionsTabInput::setData(PsiCon *psi, QWidget *)
{
    psi_ = psi;
}

void OptionsTabInput::updateDictLists()
{
    PsiOptions* o = PsiOptions::instance();
    QStringList newLoadedList = o->getOption(DICTS_OPTION).toString().split(QRegExp("\\s+"), QString::SkipEmptyParts);
    QSet<LanguageManager::LangId> newLoadedSet;
    for (auto &l: newLoadedList) {
        auto id = LanguageManager::fromString(l);
        if (id.language) {
            newLoadedSet.insert(id);
        }
    }

    newLoadedSet = (newLoadedSet.isEmpty()) ? defaultLangs_ : newLoadedSet;
    if(newLoadedSet != loadedDicts_ || loadedDicts_.isEmpty()) {
        loadedDicts_ = newLoadedSet;
    }
}

void OptionsTabInput::fillList()
{
    if(!w_) {
        return;
    }

    OptInputUI *d = (OptInputUI *)w_;

    if(!availableDicts_.isEmpty()) {
        d->availDicts->disconnect();
        d->availDicts->clear();
        for (auto const &id: availableDicts_) {
            QTreeWidgetItem *dic = new QTreeWidgetItem(d->availDicts, QTreeWidgetItem::Type);
            dic->setText(FullName, LanguageManager::languageName(id));
            dic->setData(FullName, Qt::UserRole, QVariant::fromValue<LanguageManager::LangId>(id));
            if(!loadedDicts_.contains(id)) {
                dic->setCheckState(FullName, Qt::Unchecked);
                //qDebug() << "item" << item << "unchecked";
            }
            else {
                dic->setCheckState(FullName, Qt::Checked);
                //qDebug() << "item" << item << "checked";
            }
        }
        connect(d->availDicts, &QTreeWidget::itemChanged, this, &OptionsTabInput::itemChanged);
    }
}

void OptionsTabInput::setChecked()
{
    if(!w_) {
        return;
    }

    OptInputUI *d = (OptInputUI *)w_;
    QTreeWidgetItemIterator it(d->availDicts);
    while(*it) {
        QTreeWidgetItem *item = *it;
        LanguageManager::LangId langId = item->data(FullName, Qt::UserRole).value<LanguageManager::LangId>();
        Qt::CheckState state = loadedDicts_.contains(langId) ? Qt::Checked : Qt::Unchecked;
        if(state != item->checkState(FullName)) {
            item->setCheckState(FullName, state);
        }
        ++it;
    }
}

void OptionsTabInput::itemToggled(bool toggled)
{
    if(!w_) {
        return;
    }

    OptInputUI *d = (OptInputUI *)w_;

    if(toggled) {
        updateDictLists();
        fillList();
        setChecked();
    }
    d->groupBoxDicts->setEnabled(toggled);
}

void OptionsTabInput::itemChanged(QTreeWidgetItem *item, int column)
{
    if ( !w_ )
        return;

    bool enabled = bool(item->checkState(column) == Qt::Checked);
    LanguageManager::LangId id = item->data(column, Qt::UserRole).value<LanguageManager::LangId>();
    if(loadedDicts_.contains(id) && !enabled) {
        loadedDicts_.remove(id);
    }
    else if (enabled){
        loadedDicts_ << id;
    }
    dataChanged();
}

bool OptionsTabInput::isTreeViewEmpty()
{
    if ( !w_ )
        return true;
    OptInputUI *d = (OptInputUI *)w_;
    QTreeWidgetItemIterator it(d->availDicts);
    return !bool(*it);
}
