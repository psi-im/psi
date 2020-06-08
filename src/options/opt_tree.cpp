#include "opt_tree.h"

#include "psioptionseditor.h"

#include <QVBoxLayout>

OptionsTabTree::OptionsTabTree(QObject *parent) :
    OptionsTab(parent, "tree", "", tr("Advanced"), tr("Options for advanced users"), "psi/advanced-plus")
{
    w = nullptr;
}

OptionsTabTree::~OptionsTabTree() { }

QWidget *OptionsTabTree::widget()
{
    if (w) {
        return nullptr;
    }
    w = new QWidget();
    // w->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout(w);
    // layout->setSpacing(0);
    layout->setMargin(0);

    QLabel *lb = new QLabel(tr("Please note: This editor will change the options "
                               "directly. Pressing Cancel will not revert these changes."),
                            w);
    lb->setWordWrap(true);
    layout->addWidget(lb);

    PsiOptionsEditor *poe = new PsiOptionsEditor(w);
    layout->addWidget(poe);
    poe->show();

    return w;
}
