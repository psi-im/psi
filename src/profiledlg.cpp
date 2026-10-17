/*
 * profiledlg.cpp - dialogs for manipulating profiles
 * Copyright (C) 2001-2003  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "profiledlg.h"

#include "applicationinfo.h"
#include "common.h"
#include "iconset.h"
#include "profiles.h"
#include "psioptions.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>

#include <functional>

namespace {

bool validProfileName(const QString &name)
{
    if (name.isEmpty())
        return false;
    for (const QChar &ch : name) {
        if (!ch.isLetterOrNumber())
            return false;
    }
    return true;
}

class StretchLogoLabel : public QLabel {
public:
    StretchLogoLabel(const QPixmap &pix, QWidget *label) : QLabel(label->parentWidget()), pixmap_(pix)
    {
        replaceWidget(label, this);
        setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum));
    }

    QSize sizeHint() const override
    {
        QSize sh = QLabel::sizeHint();
        sh.setHeight(pixmap_.height());
        return sh;
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter p(this);
        p.fillRect(rect(), Qt::red);
        p.drawTiledPixmap(0, 0, width(), height(), pixmap_);
    }

private:
    QPixmap pixmap_;
};

// Keep native list editing (double-click/F2), but reserve the trailing
// area of each row for a delete action.
class ProfileItemDelegate : public QStyledItemDelegate {
public:
    explicit ProfileItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
    {
        removeIcon_ = IconsetFactory::icon("psi/remove").icon();
    }

    std::function<void(int)> deleteClicked;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem textOption = option;
        textOption.rect.setRight(deleteArea(option).left() - 1);
        QStyledItemDelegate::paint(painter, textOption, index);

        if (index.flags() & Qt::ItemIsEnabled) {
            QRect iconRect(QPoint(0, 0), QSize(16, 16));
            iconRect.moveCenter(deleteArea(option).center());
            removeIcon_.paint(painter, iconRect);
        }
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (deleteArea(option).contains(mouse->pos())) {
                if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton
                    && deleteClicked) {
                    deleteClicked(index.row());
                }
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override
    {
        if (deleteArea(option).contains(event->pos())) {
            QToolTip::showText(event->globalPos(), tr("Delete profile"), view);
            return true;
        }
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto *line = qobject_cast<QLineEdit *>(editor)) {
            line->setValidator(new QRegularExpressionValidator(
                QRegularExpression(QStringLiteral("[\\p{L}\\p{N}]+")), line));
        }
        return editor;
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
    {
        QStyleOptionViewItem textOption = option;
        textOption.rect.setRight(deleteArea(option).left() - 1);
        QStyledItemDelegate::updateEditorGeometry(editor, textOption, index);
    }

private:
    QRect deleteArea(const QStyleOptionViewItem &option) const
    {
        return QRect(option.rect.right() - 27, option.rect.top(), 28, option.rect.height());
    }

    QIcon removeIcon_;
};

} // namespace

ProfileOpenDlg::ProfileOpenDlg(const QString &preferred, const VarList &langs, const QString &curLang,
                               QWidget *parent) : QDialog(parent), _preferredProfile(preferred)
{
    setupUi(this);
    setWindowTitle(CAP(windowTitle()));
    setWindowIcon(IconsetFactory::icon("psi/profile").icon());
    pb_add->setIcon(IconsetFactory::icon("psi/add").icon());
    pb_add->setIconSize(QSize(16, 16));

    const QPixmap logo = IconsetFactory::icon("psi/logo").pixmap();
    lb_logo->setPixmap(logo);
    lb_logo->setFixedSize(logo.size());
    lb_logo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const QImage logoImage = logo.toImage();
    new StretchLogoLabel(
        QPixmap::fromImage(logoImage.copy(logoImage.width() - 1, 0, 1, logoImage.height())), lb_right);

    for (const auto &lang : langs) {
        cb_lang->addItem(lang.data(), lang.key());
    }
    for (int i = 0; i < cb_lang->count(); ++i) {
        if (cb_lang->itemData(i).toString() == curLang) {
            cb_lang->setCurrentIndex(i);
            break;
        }
    }

    auto *delegate = new ProfileItemDelegate(lbx_profiles);
    delegate->deleteClicked = [this](int row) {
        if (auto *item = lbx_profiles->item(row)) {
            const QString name = item->data(Qt::UserRole).toString();
            // Don't remove list items from inside the delegate's event handler.
            QTimer::singleShot(0, this, [this, name]() { confirmDelete(name); });
        }
    };
    lbx_profiles->setItemDelegate(delegate);
    lbx_profiles->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    lbx_profiles->setToolTip(tr("Double-click or press F2 to rename a profile. Press Delete to remove it."));
    le_newProfile->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[\\p{L}\\p{N}]+")), le_newProfile));

    auto *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), lbx_profiles);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        if (auto *item = lbx_profiles->currentItem())
            confirmDelete(item->data(Qt::UserRole).toString());
    });

    connect(le_newProfile, &QLineEdit::textChanged, this, &ProfileOpenDlg::updateAddButton);
    connect(le_newProfile, &QLineEdit::returnPressed, this, &ProfileOpenDlg::addProfile);
    connect(pb_add, &QToolButton::clicked, this, &ProfileOpenDlg::addProfile);
    connect(lbx_profiles, &QListWidget::currentRowChanged, this, &ProfileOpenDlg::updateSelection);
    connect(lbx_profiles, &QListWidget::itemChanged, this, &ProfileOpenDlg::profileRenamed);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (!selectedProfile().isEmpty())
            accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    cb_lang->setToolTip(tr("Choose the application language. This setting applies to all profiles."));
    ck_auto->setToolTip(tr("Open the selected profile automatically the next time Psi starts."));

    // Align the secondary hint with the checkbox text, using the active Qt style.
    const int hintIndent = ck_auto->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, ck_auto)
        + ck_auto->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, ck_auto);
    lb_startupHint->setIndent(hintIndent);

    QFont hintFont = lb_startupHint->font();
    if (hintFont.pointSizeF() > 0)
        hintFont.setPointSizeF(hintFont.pointSizeF() * 0.8);
    else if (hintFont.pixelSize() > 0)
        hintFont.setPixelSize(qMax(1, qRound(hintFont.pixelSize() * 0.8)));
    lb_startupHint->setFont(hintFont);

    QPalette hintPalette = lb_startupHint->palette();
    QColor hintColor = hintPalette.color(lb_startupHint->foregroundRole());
    hintColor.setAlphaF(0.65);
    hintPalette.setColor(lb_startupHint->foregroundRole(), hintColor);
    lb_startupHint->setPalette(hintPalette);

    buttonBox->button(QDialogButtonBox::Open)->setDefault(true);
    reload(preferred);
}

QString ProfileOpenDlg::selectedProfile() const
{
    auto *item = lbx_profiles->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString ProfileOpenDlg::preferredProfile() const { return _preferredProfile; }

QString ProfileOpenDlg::selectedLanguage() const { return cb_lang->currentData().toString(); }

void ProfileOpenDlg::reload(const QString &preferred)
{
    lbx_profiles->clear();
    for (const QString &name : getProfilesList()) {
        auto *item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        lbx_profiles->addItem(item);
    }

    int row = 0;
    if (!preferred.isEmpty()) {
        for (int i = 0; i < lbx_profiles->count(); ++i) {
            if (lbx_profiles->item(i)->text() == preferred) {
                row = i;
                break;
            }
        }
    }
    if (lbx_profiles->count())
        lbx_profiles->setCurrentRow(row);
    updateSelection();
    updateAddButton();
}

void ProfileOpenDlg::updateAddButton()
{
    const QString name = le_newProfile->text();
    pb_add->setEnabled(validProfileName(name) && !profileExists(name));
}

void ProfileOpenDlg::addProfile()
{
    if (!pb_add->isEnabled())
        return;

    const QString name = le_newProfile->text();
    if (!profileNew(name)) {
        lb_feedback->setText(tr("Unable to create this profile. Check folder permissions."));
        return;
    }

    PsiOptions options;
    const bool initialized = options.newProfile();
    const bool saved = options.save(pathToProfile(name, ApplicationInfo::ConfigLocation) + "/options.xml");
    if (!initialized || !saved)
        lb_feedback->setText(tr("Profile created, but its default options could not be saved."));
    else
        lb_feedback->clear();

    auto *item = new QListWidgetItem(name);
    item->setData(Qt::UserRole, name);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    lbx_profiles->addItem(item);
    lbx_profiles->sortItems();
    lbx_profiles->setCurrentItem(item);
    le_newProfile->clear();
}

void ProfileOpenDlg::profileRenamed(QListWidgetItem *item)
{
    const QString oldName = item->data(Qt::UserRole).toString();
    const QString newName = item->text();
    if (oldName == newName)
        return;

    QString error;
    if (!validProfileName(newName))
        error = tr("Profile names may contain only letters and numbers.");
    else if (profileExists(newName))
        error = tr("A profile with this name already exists.");
    else if (!profileRename(oldName, newName))
        error = tr("Unable to rename the profile. Check folder permissions.");

    if (!error.isEmpty()) {
        QSignalBlocker blocker(lbx_profiles);
        item->setText(oldName);
        lb_feedback->setText(error);
        return;
    }

    item->setData(Qt::UserRole, newName);
    if (_preferredProfile == oldName)
        _preferredProfile = newName;
    lb_feedback->clear();
    lbx_profiles->sortItems();
    updateSelection();
    updateAddButton();
}

void ProfileOpenDlg::confirmDelete(const QString &name)
{
    if (!profileExists(name))
        return;

    auto *confirm = new QMessageBox(
        QMessageBox::Warning, CAP(tr("Delete Profile")),
        tr("Delete profile \"%1\"?\n\nAll of its message history, settings, and cached files will be deleted.")
            .arg(name),
        QMessageBox::Yes | QMessageBox::No, this);
    confirm->setDefaultButton(QMessageBox::No);
    confirm->setAttribute(Qt::WA_DeleteOnClose);
    connect(confirm, &QMessageBox::finished, this, [this, name](int result) {
        if (result == QMessageBox::Yes)
            deleteProfile(name);
    });
    confirm->open();
}

void ProfileOpenDlg::deleteProfile(const QString &name)
{
    QStringList paths;
    for (const auto location : { ApplicationInfo::ConfigLocation, ApplicationInfo::DataLocation,
                                 ApplicationInfo::CacheLocation }) {
        const QString path = pathToProfile(name, location);
        if (!paths.contains(path))
            paths << path;
    }

    if (!profileDelete(paths)) {
        lb_feedback->setText(tr("Unable to delete the profile completely. Check folder permissions."));
        return;
    }

    for (int i = 0; i < lbx_profiles->count(); ++i) {
        if (lbx_profiles->item(i)->data(Qt::UserRole).toString() == name) {
            delete lbx_profiles->takeItem(i);
            if (lbx_profiles->count())
                lbx_profiles->setCurrentRow(qMin(i, lbx_profiles->count() - 1));
            break;
        }
    }
    if (_preferredProfile == name)
        _preferredProfile.clear();
    lb_feedback->clear();
    updateSelection();
    updateAddButton();
}

void ProfileOpenDlg::updateSelection()
{
    const QString name = selectedProfile();
    buttonBox->button(QDialogButtonBox::Open)->setEnabled(!name.isEmpty());
    ck_auto->setEnabled(!name.isEmpty());
}
