#include <QInputDialog>
#include <QMessageBox>

#include "sendbuttonmenu.h"
#include "psioptions.h"
#include "psiiconset.h"
#include "common.h"

SendButtonTemplatesEditor::SendButtonTemplatesEditor(QWidget* parent)
    : QDialog(parent)
{
    ui_.setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    // Create actions
    QAction *addRootTemplAction = new QAction(tr("Add template string"), this);
    connect(addRootTemplAction, SIGNAL(triggered()), this, SLOT(addRootTemplate()));
    addChildTemplAction = new QAction(tr("Add template string as submenu"), this);
    connect(addChildTemplAction, SIGNAL(triggered()), this, SLOT(addChildTemplate()));
    QAction *addRootSeparatorAction = new QAction(tr("Add separator"), this);
    connect(addRootSeparatorAction, SIGNAL(triggered()), this, SLOT(addRootSeparator()));
    addChildSeparatorAction = new QAction(tr("Add separator as submenu"), this);
    connect(addChildSeparatorAction, SIGNAL(triggered()), this, SLOT(addChildSeparator()));
    editAction = new QAction(IconsetFactory::icon("psi/action_templates_edit").icon(), tr("Edit"), this);
    removeAction = new QAction(IconsetFactory::icon("psi/remove").icon(), tr("Remove"), this);
    upAction = new QAction(IconsetFactory::icon("psi/arrowUp").icon(), tr("Up"), this);
    downAction = new QAction(IconsetFactory::icon("psi/arrowDown").icon(), tr("Down"), this);
    // Create Add button menu
    QMenu *add_menu = new QMenu(this);
    add_menu->addAction(addRootTemplAction);
    add_menu->addAction(addChildTemplAction);
    add_menu->addAction(addRootSeparatorAction);
    add_menu->addAction(addChildSeparatorAction);
    ui_.btnAdd->setMenu(add_menu);
    //--
    QStringList templ_list = PsiOptions::instance()->getOption("options.ui.chat.templates", QStringList()).toStringList();
    int templ_cnt = templ_list.size();
    QHash<QString, QTreeWidgetItem *> subitems;
    for (int i = 0; i < templ_cnt; i++) {
        QStringList menu_list = SendButtonTemplatesMenu::getMenuItems(templ_list.at(i));
        if (menu_list.size() == 0)
            continue;
        // find subitems if exists
        QTreeWidgetItem *item = nullptr;
        QString menu_str = "";
        int j = menu_list.size() - 2;
        for (; j >= 0; j--) {
            menu_str = "";
            QString menu_str2 = "";
            for (int k = 0; k <= j; k++) {
                menu_str = menu_str2;
                menu_str2.append("/" + menu_list.at(k));
            }
            item = subitems.value(menu_str2, nullptr);
            if (item) {
                menu_str = menu_str2;
                ++j;
                break;
            }
        }
        if (j < 0)
            j = 0;
        // create subitems
        int sub_cnt = menu_list.size();
        for (; j < sub_cnt; j++) {
            QString str1 = menu_list.at(j);
            QTreeWidgetItem::ItemType type = QTreeWidgetItem::Type;
            if (j == (sub_cnt - 1) && str1 == "\\-") {
                str1 = tr("<separator>");
                type = QTreeWidgetItem::UserType;
            }
            if (j == 0) {
                // toplevel item
                item = new QTreeWidgetItem(ui_.lstTemplates, type);
            } else {
                item = new QTreeWidgetItem(item, type);
            }
            if (type == QTreeWidgetItem::UserType) {
                item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
            }
            item->setText(0, SendButtonTemplatesMenu::stripSlashes(str1));
            if (j < (sub_cnt - 1)) {
                menu_str.append("/" + str1);
                subitems[menu_str] = item;
            }
        }
    }
}

void SendButtonTemplatesEditor::addRootTemplate()
{
    QString t = ui_.txtTemplate->text();
    if (t.isEmpty())
        return;
    QTreeWidgetItem *item = new QTreeWidgetItem(ui_.lstTemplates, QStringList(t), QTreeWidgetItem::Type);
    ui_.lstTemplates->addTopLevelItem(item);
    ui_.txtTemplate->setText("");
}

void SendButtonTemplatesEditor::addChildTemplate()
{
    QString t = ui_.txtTemplate->text();
    if (t.isEmpty())
        return;
    QTreeWidgetItem *curr_item = ui_.lstTemplates->currentItem();
    if (curr_item && curr_item->type() == QTreeWidgetItem::Type) {
        curr_item->addChild(new QTreeWidgetItem(QStringList(t), QTreeWidgetItem::Type));
        ui_.txtTemplate->setText("");
    }
}

void SendButtonTemplatesEditor::addRootSeparator()
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui_.lstTemplates, QStringList(tr("<separator>")), QTreeWidgetItem::UserType);
    item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
    ui_.lstTemplates->addTopLevelItem(item);
}

void SendButtonTemplatesEditor::addChildSeparator()
{
    QTreeWidgetItem *curr_item = ui_.lstTemplates->currentItem();
    if (curr_item && curr_item->type() == QTreeWidgetItem::Type) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(tr("<separator>")), QTreeWidgetItem::UserType);
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        curr_item->addChild(item);
    }
}

void SendButtonTemplatesEditor::removeTemplate()
{
    QTreeWidgetItem *item = ui_.lstTemplates->currentItem();
    if (item) {
        delete item;
    }
}

void SendButtonTemplatesEditor::editTemplate()
{
    QTreeWidgetItem *item = ui_.lstTemplates->currentItem();
    if (item && item->type() == QTreeWidgetItem::Type) {
        QString templ_str = item->text(0);
        QInputDialog *editBox = new QInputDialog(this, Qt::Dialog);
        editBox->setWindowTitle(tr("Edit template"));
        editBox->setLabelText(tr("Input new template text"));
        editBox->setTextEchoMode(QLineEdit::Normal);
        editBox->setTextValue(templ_str);
        editBox->setWindowModality(Qt::WindowModal);
        if (editBox->exec() == QInputDialog::Accepted) {
            QString new_templ_str = editBox->textValue();
            if (!new_templ_str.isEmpty() && new_templ_str != templ_str) {
                item->setText(0, new_templ_str);
            }
        }
        delete editBox;
    }
}

void SendButtonTemplatesEditor::upTemplate()
{
    swapItem(-1);
}

void SendButtonTemplatesEditor::downTemplate()
{
    swapItem(1);
}

void SendButtonTemplatesEditor::selectionChanged(QTreeWidgetItem *current, QTreeWidgetItem */*previous*/)
{
    bool up_e = false;
    bool dw_e = false;
    bool ed_e = false;
    bool dl_e = false;
    QModelIndex index = ui_.lstTemplates->currentIndex();
    if (current && index.isValid()) {
        QTreeWidgetItem *parent = current->parent();
        int cnt = 0;
        if (parent) {
            cnt = parent->childCount();
        } else {
            cnt = ui_.lstTemplates->topLevelItemCount();
        }
        int row = index.row();
        if (cnt > 0) {
            if (row > 0)
                up_e = true;
            if (row < cnt - 1)
                dw_e = true;
            if (current->type() == QTreeWidgetItem::Type)
                ed_e = true;
            dl_e = true;
        }

    }
    addChildTemplAction->setEnabled(ed_e);
    addChildSeparatorAction->setEnabled(ed_e);
    ui_.btnEdit->setEnabled(ed_e);
    editAction->setEnabled(ed_e);
    ui_.btnRemove->setEnabled(dl_e);
    removeAction->setEnabled(ed_e);
    ui_.btnUp->setEnabled(up_e);
    upAction->setEnabled(up_e);
    ui_.btnDown->setEnabled(dw_e);
    downAction->setEnabled(dw_e);
}

void SendButtonTemplatesEditor::contextMenu(QPoint pos)
{
    QMenu* menu = new QMenu(this);
    menu->addAction(editAction);
    menu->addAction(removeAction);
    menu->addSeparator();
    menu->addAction(upAction);
    menu->addAction(downAction);
    QAction *result = menu->exec(ui_.lstTemplates->mapToGlobal(pos));
    if (result == editAction) {
        editTemplate();
    } else if (result == removeAction) {
        removeTemplate();
    } else if (result == upAction) {
        upTemplate();
    } else if (result == downAction) {
        downTemplate();
    }
    delete menu;
}

void SendButtonTemplatesEditor::swapItem(int updown)
{
    QTreeWidgetItem *item = ui_.lstTemplates->currentItem();
    QModelIndex index = ui_.lstTemplates->currentIndex();
    if (item && index.isValid()) {
        int row = index.row();
        int new_pos = row + updown;
        QTreeWidgetItem * parent = item->parent();
        int cnt = 0;
        if (parent) {
            cnt = parent->childCount();
        } else {
            cnt = ui_.lstTemplates->topLevelItemCount();
        }
        if (new_pos >= 0 && new_pos < cnt) {
            if (parent) {
                parent->removeChild(item);
                parent->insertChild(new_pos, item);
            } else {
                ui_.lstTemplates->takeTopLevelItem(row);
                ui_.lstTemplates->insertTopLevelItem(new_pos, item);
            }
            ui_.lstTemplates->setCurrentItem(item);
            selectionChanged(item, item); // needed update buttons
        }
    }
}

QStringList SendButtonTemplatesEditor::genTemplatesList(QTreeWidgetItem* item, QString base_path)
{
    QStringList res_lst;
    QString new_base = base_path;
    if (!new_base.isEmpty())
        new_base.append("/");
    QString item_text = SendButtonTemplatesMenu::addSlashes(item->text(0));
    int child_cnt = item->childCount();
    if (child_cnt == 0) {
        if (item->type() != QTreeWidgetItem::Type)
            item_text = "\\-"; // Separator
        res_lst.push_back(new_base + item_text);
        return res_lst;
    }
    new_base.append(item_text);
    for (int idx = 0; idx < child_cnt; idx++) {
        res_lst.append(genTemplatesList(item->child(idx), new_base));
    }
    return res_lst;
}

void SendButtonTemplatesEditor::accept()
{
    QString t = ui_.txtTemplate->text().trimmed();
    if (!t.isEmpty()) {
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setWindowTitle(tr("Save templates"));
        msgBox->setText(tr("The template \"%1\" hasn't been saved!").arg(t));
        msgBox->setInformativeText(tr("Continue?"));
        msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox->setDefaultButton(QMessageBox::No);
        msgBox->setWindowModality(Qt::WindowModal);
        int res = msgBox->exec();
        delete msgBox;
        if (res == QMessageBox::No) {
            return;
        }
    }
    QStringList templates;
    int top_cnt = ui_.lstTemplates->topLevelItemCount();
    for (int idx = 0; idx < top_cnt; idx++) {
        templates.append(genTemplatesList(ui_.lstTemplates->topLevelItem(idx)));
    }
    PsiOptions::instance()->setOption("options.ui.chat.templates", templates);
    QDialog::accept();
    close();
}

void SendButtonTemplatesEditor::reject()
{
    QDialog::reject();
    close();
}

//------------------------------

SendButtonTemplatesMenu::SendButtonTemplatesMenu(QWidget* parent)
    : QMenu(parent)
    , ps_(false)
{
    setSeparatorsCollapsible(true);
    update();
    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
}

void SendButtonTemplatesMenu::setParams(bool ps)
{
    if (ps_ != ps) {
        ps_ = ps;
        updatePsAction();
    }
}

void SendButtonTemplatesMenu::updatePsAction()
{
    bool ps_v = false;
    if (ps_ && !PsiOptions::instance()->getOption("options.ui.chat.disable-paste-send").toBool())
        ps_v = true;
    pasteSendAct->setVisible(ps_v);
}

/**
 * Split menu text string as submenu. Char '/' used as separator.
 */
QStringList SendButtonTemplatesMenu::getMenuItems(const QString text)
{
    QStringList res_list;
    int str_len = text.length();
    unsigned int sl_cnt = 0;
    int str_pos = 0;
    for (int i = 0; i < str_len; i++) {
        QChar ch = text.at(i);
        if (ch == '\\') {
            sl_cnt++;
        } else if (ch == '/') {
            if ((sl_cnt & 1) == 0) {
                QString str1 = text.mid(str_pos, i - str_pos).trimmed();
                if (!str1.isEmpty()) {
                    res_list.push_back(str1);
                    str_pos = i + 1;
                }
                str_pos = i + 1;
            }
            sl_cnt = 0;
        } else {
            sl_cnt = 0;
        }
    }
    if (str_pos < str_len) {
        QString str1 = text.mid(str_pos);
        if (!str1.isEmpty()) {
            res_list.push_back(str1);
        }
    }
    return res_list;
}

QAction *SendButtonTemplatesMenu::makeAction(QString text)
{
    QString str1;
    if (text.length() > 30) {
        str1 = text.left(30).replace("&", "&&") + "...";
    } else {
        str1 = text.replace("&", "&&");
    }
    QAction *act = new QAction(str1, this);
    return act;
}

void SendButtonTemplatesMenu::update()
{
    clearMenu(this);

    pasteSendAct = new QAction(IconsetFactory::icon("psi/action_paste_and_send").icon(), tr("Paste and &Send"), this);
    connect(pasteSendAct, SIGNAL(triggered()), SIGNAL(doPasteAndSend()));
    addAction(pasteSendAct);
    updatePsAction();
    addSeparator();

    QAction *editTemplatesAct = new QAction(IconsetFactory::icon("psi/action_templates_edit").icon(), tr("&Edit Templates"), this);
    connect(editTemplatesAct, SIGNAL(triggered()), SIGNAL(doEditTemplates()));
    addAction(editTemplatesAct);

    onlyPaste = new QAction(tr("Only &Paste"), this);
    onlyPaste->setCheckable(true);
    onlyPaste->setChecked(PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool());
    connect(onlyPaste, SIGNAL(triggered()), SLOT(clickOnlyPaste()));
    addAction(onlyPaste);
    addSeparator();

    QStringList templ_list = PsiOptions::instance()->getOption("options.ui.chat.templates", QStringList()).toStringList();
    int templ_cnt = templ_list.size();
    QHash<QString, QAction *> submenus;
    for (int i = 0; i < templ_cnt; i++) {
        QStringList menu_list = SendButtonTemplatesMenu::getMenuItems(templ_list.at(i));
        if (menu_list.size() == 0)
            continue;
        // find submenus if exists
        QAction *m_act = nullptr;
        QString menu_str = "";
        int j = menu_list.size() - 2;
        for (; j >= 0; j--) {
            menu_str = "";
            QString menu_str2 = "";
            for (int k = 0; k <= j; k++) {
                menu_str = menu_str2;
                menu_str2.append("/" + menu_list.at(k));
            }
            m_act = submenus.value(menu_str2, nullptr);
            if (m_act) {
                menu_str = menu_str2;
                ++j;
                break;
            }
        }
        if (j < 0)
            j = 0;
        // create submenus
        QMenu *smenu = this;
        if (m_act) {
            smenu = m_act->menu();
            if (!smenu)
                smenu = new QMenu(this);
        }
        int sub_cnt = menu_list.size();
        for (; ; j++) {
            QString str1 = menu_list.at(j);
            if (j == (sub_cnt - 1) && str1 == "\\-") {
                smenu->addSeparator();
                break;
            }
            QAction *c_act = makeAction(SendButtonTemplatesMenu::stripSlashes(str1));
            smenu->addAction(c_act);
            if (j == (sub_cnt - 1)) {
                connect(c_act, &QAction::triggered, this, [this, str1](){
                    emit doTemplateText(SendButtonTemplatesMenu::stripSlashes(str1));
                });
                break;
            } else {
                smenu = new QMenu(this);
                c_act->setMenu(smenu);
                menu_str.append("/" + str1);
                submenus[menu_str] = c_act;
            }
        }
    }
}

void SendButtonTemplatesMenu::clickOnlyPaste()
{
    if (PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool())
        PsiOptions::instance()->setOption("options.ui.chat.only-paste-template", false);
    else
        PsiOptions::instance()->setOption("options.ui.chat.only-paste-template", true);
}

void SendButtonTemplatesMenu::optionChanged(const QString& option)
{
    if (option == "options.ui.chat.disable-paste-send") {
        updatePsAction();
    } else if (option == "options.ui.chat.css") {
        setStyleSheet(PsiOptions::instance()->getOption("options.ui.chat.css").toString());
    //} else if (option == "options.ui.chat.only-paste-template") {
    //    onlyPaste->setChecked(PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool());
    }
}

QString SendButtonTemplatesMenu::addSlashes(QString str)
{
    return str.replace("\\", "\\\\", Qt::CaseSensitive).replace("/", "\\/");
}

QString SendButtonTemplatesMenu::stripSlashes(QString str)
{
    return str.replace("\\/", "/").replace("\\\\", "\\");
}
