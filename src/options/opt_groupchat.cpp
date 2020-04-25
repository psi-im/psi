#include "opt_groupchat.h"

#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "ui_opt_general_groupchat.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QRadioButton>
#include <QSignalMapper>

class GeneralGroupchatUI : public QWidget, public Ui::GeneralGroupchat {
public:
    GeneralGroupchatUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabGroupchat -- TODO: simplify the code
//----------------------------------------------------------------------------

OptionsTabGroupchat::OptionsTabGroupchat(QObject *parent) :
    OptionsTab(parent, "groupchat", "", tr("Groupchat"), tr("Configure the groupchat"), "psi/groupChat")
{
}

void OptionsTabGroupchat::setData(PsiCon *, QWidget *_dlg) { dlg = _dlg; }

QWidget *OptionsTabGroupchat::widget()
{
    if (w)
        return nullptr;

    w                     = new GeneralGroupchatUI();
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);

    connect(d->pb_nickColor, &QPushButton::clicked, this, &OptionsTabGroupchat::chooseGCNickColor);
    connect(d->lw_nickColors, &QListWidget::currentItemChanged, this, &OptionsTabGroupchat::selectedGCNickColor);

    connect(d->pb_addHighlightWord, &QPushButton::clicked, this, &OptionsTabGroupchat::addGCHighlight);
    connect(d->pb_removeHighlightWord, &QPushButton::clicked, this, &OptionsTabGroupchat::removeGCHighlight);

    connect(d->pb_addNickColor, &QPushButton::clicked, this, &OptionsTabGroupchat::addGCNickColor);
    connect(d->pb_removeNickColor, &QPushButton::clicked, this, &OptionsTabGroupchat::removeGCNickColor);

    connect(d->alertGroupBox, &QGroupBox::toggled, this, [this]() { updateWidgetsState(); });
    connect(d->cb_coloringType, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](const int index) {
                if (index >= 0)
                    updateWidgetsState();
            });

    return w;
}

void OptionsTabGroupchat::applyOptions()
{
    if (!w)
        return;

    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    PsiOptions *        o = PsiOptions::instance();
    o->setOption("options.ui.muc.use-highlighting", d->alertGroupBox->isChecked());
    const int index = d->cb_coloringType->currentIndex();
    o->setOption("options.ui.muc.use-nick-coloring", index != NONE);
    o->setOption("options.ui.muc.use-hash-nick-coloring", index == HASH);
    o->setOption("options.muc.show-joins", d->ck_showJoins->isChecked());
    o->setOption("options.ui.muc.show-initial-joins", d->ck_showInitialJoins->isChecked());
    o->setOption("options.muc.show-status-changes", d->ck_showStatusChanges->isChecked());
    o->setOption("options.ui.muc.status-with-priority", d->ck_showStatusPriority->isChecked());

    QStringList highlight;
    int         i;
    for (i = 0; i < int(d->lw_highlightWords->count()); i++)
        highlight << d->lw_highlightWords->item(i)->text();
    o->setOption("options.ui.muc.highlight-words", highlight);

    QStringList colors;
    for (i = 0; i < int(d->lw_nickColors->count()); i++)
        colors << d->lw_nickColors->item(i)->text();
    o->setOption("options.ui.look.colors.muc.nick-colors", colors);
}

void OptionsTabGroupchat::restoreOptions()
{
    if (!w)
        return;

    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    PsiOptions *        o = PsiOptions::instance();

    d->cb_coloringType->setCurrentIndex(-1); // deselect combobox items
    // no need to call dataChanged() when these widgets are modified
    disconnect(d->le_newNickColor, &QLineEdit::textChanged, nullptr, nullptr);
    disconnect(d->le_newHighlightWord, &QLineEdit::textChanged, nullptr, nullptr);
    connect(d->le_newNickColor, &QLineEdit::textChanged, this, &OptionsTabGroupchat::displayGCNickColor);

    d->alertGroupBox->setChecked(o->getOption("options.ui.muc.use-highlighting").toBool());
    bool hashNickColoring = o->getOption("options.ui.muc.use-hash-nick-coloring").toBool();
    int  index            = NONE;
    if (o->getOption("options.ui.muc.use-nick-coloring").toBool()) {
        index = hashNickColoring ? HASH : MANUAL;
    }
    d->cb_coloringType->setCurrentIndex(index);
    d->lw_highlightWords->clear();
    d->lw_highlightWords->addItems(o->getOption("options.ui.muc.highlight-words").toStringList());
    d->lw_nickColors->clear();
    d->ck_showJoins->setChecked(o->getOption("options.muc.show-joins").toBool());
    d->ck_showInitialJoins->setChecked(o->getOption("options.ui.muc.show-initial-joins").toBool());
    d->ck_showStatusChanges->setChecked(o->getOption("options.muc.show-status-changes").toBool());
    d->ck_showStatusPriority->setChecked(o->getOption("options.ui.muc.status-with-priority").toBool());

    for (QString col : o->getOption("options.ui.look.colors.muc.nick-colors").toStringList()) {
        addNickColor(col);
    }

    d->le_newHighlightWord->setText("");
    d->le_newNickColor->setText("#FFFFFF");
}

void OptionsTabGroupchat::updateWidgetsState()
{
    if (!w)
        return;

    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);

    if (sender() == d->alertGroupBox) {
        bool enableHighlights = d->alertGroupBox->isChecked();
        d->lw_highlightWords->setEnabled(enableHighlights);
        d->le_newHighlightWord->setEnabled(enableHighlights);
        d->pb_addHighlightWord->setEnabled(enableHighlights);
        d->pb_removeHighlightWord->setEnabled(enableHighlights);
        emit dataChanged();
        return;
    }

    {
        bool enableNickColors = d->cb_coloringType->currentIndex() == MANUAL;
        d->lw_nickColors->setEnabled(enableNickColors);
        d->le_newNickColor->setEnabled(enableNickColors);
        d->pb_addNickColor->setEnabled(enableNickColors);
        d->pb_removeNickColor->setEnabled(enableNickColors);
        d->pb_nickColor->setEnabled(enableNickColors);
    }
}

static QPixmap name2color(QString name)
{
    QColor   c(name);
    QPixmap  pix(16, 16);
    QPainter p(&pix);

    p.fillRect(0, 0, pix.width(), pix.height(), QBrush(c));
    p.setPen(QColor(0, 0, 0));
    p.drawRect(0, 0, pix.width(), pix.height());
    p.end();

    return pix;
}

void OptionsTabGroupchat::addNickColor(QString name)
{
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    d->lw_nickColors->addItem(new QListWidgetItem(name2color(name), name));
}

void OptionsTabGroupchat::addGCHighlight()
{
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    if (d->le_newHighlightWord->text().isEmpty())
        return;

    d->lw_highlightWords->addItem(d->le_newHighlightWord->text());
    d->le_newHighlightWord->setFocus();
    d->le_newHighlightWord->setText("");

    emit dataChanged();
}

void OptionsTabGroupchat::removeGCHighlight()
{
    GeneralGroupchatUI *d  = static_cast<GeneralGroupchatUI *>(w);
    int                 id = d->lw_highlightWords->currentRow();
    if (id == -1)
        return;

    d->lw_highlightWords->takeItem(id);

    emit dataChanged();
}

void OptionsTabGroupchat::addGCNickColor()
{
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    if (d->le_newNickColor->text().isEmpty())
        return;

    addNickColor(d->le_newNickColor->text());
    d->le_newNickColor->setFocus();
    d->le_newNickColor->setText("");

    emit dataChanged();
}

void OptionsTabGroupchat::removeGCNickColor()
{
    GeneralGroupchatUI *d  = static_cast<GeneralGroupchatUI *>(w);
    int                 id = d->lw_nickColors->currentRow();
    if (id == -1)
        return;

    d->lw_nickColors->takeItem(id);

    emit dataChanged();
}

void OptionsTabGroupchat::chooseGCNickColor()
{
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    QColor              c = QColorDialog::getColor(QColor(d->le_newNickColor->text()), dlg);
    if (c.isValid()) {
        QString cs = c.name();
        d->le_newNickColor->setText(cs);
    }
}

void OptionsTabGroupchat::selectedGCNickColor(QListWidgetItem *item)
{
    if (!item)
        return; // no selection on empty list
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    d->le_newNickColor->setText(item->text());
}

void OptionsTabGroupchat::displayGCNickColor()
{
    GeneralGroupchatUI *d = static_cast<GeneralGroupchatUI *>(w);
    d->pb_nickColor->setIcon(name2color(d->le_newNickColor->text()));
}
