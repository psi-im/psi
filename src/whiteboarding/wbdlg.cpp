/*
 * wbdlg.cpp - dialog for whiteboarding
 * Copyright (C) 2006  Joonas Govenius
 *
 * Originally developed from:
 * chatdlg.cpp - dialog for handling chats
 * Copyright (C) 2001-2002  Justin Karneges
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

#include "wbdlg.h"

#include "accountlabel.h"
#include "iconaction.h"
#include "iconset.h"
#include "psiaccount.h"
#include "stretchwidget.h"

#include <QActionGroup>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <iris/jingle-session.h>

static const QString geometryOption = "options.ui.chat.wb-size";

//----------------------------------------------------------------------------
// WbDlg
//----------------------------------------------------------------------------

WbDlg::WbDlg(XMPP::Jingle::SXE::Application *application, PsiAccount *pa) :
    application_(application), document_(application->document())
{
    // Keep the document readable and savable after the session ends.
    document_->setParent(this);
    peer_       = application->pad()->session()->peer();
    sid_        = application->pad()->session()->sid();
    groupChat_  = false;
    pending_    = 0;
    keepOpen_   = false;
    allowEdits_ = true;

    selfDestruct_ = nullptr;
    setAttribute(Qt::WA_DeleteOnClose, false); // we want deferred endSession call and delete from manager

    setWindowTitle(CAP(tr("Whiteboard (%1)").arg(pa->jid().bare())));
    QVBoxLayout *vb1 = new QVBoxLayout(this);

    // first row
    le_jid_ = new QLineEdit(this);
    le_jid_->setReadOnly(true);
    le_jid_->setFocusPolicy(Qt::NoFocus);
    le_jid_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    lb_ident_ = new AccountLabel(this);
    lb_ident_->setAccount(pa);
    lb_ident_->setShowJid(false);
    lb_ident_->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
    QHBoxLayout *hb1 = new QHBoxLayout();
    hb1->addWidget(le_jid_);
    hb1->addWidget(lb_ident_);
    vb1->addLayout(hb1);

    // mid area
    wbWidget_ = new WbWidget(document_, this);
    wbWidget_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    vb1->addWidget(wbWidget_);

    // Bottom (tool) area
    act_save_ = new IconAction(tr("Save session"), "psi/saveBoard", tr("Save the contents of the whiteboard"), 0, this);
    act_geometry_ = new IconAction(tr("Change the geometry"), "psi/whiteboard", tr("Change the geometry"), 0, this);
    act_clear_    = new IconAction(tr("Clear the whiteboard for everyone"), "psi/clearChat", tr("Clear the whiteboard"),
                                   0, this);

    // Black is the default color
    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(Qt::black));
    act_color_ = new QAction(QIcon(pixmap), tr("Stroke color"), this);
    pixmap.fill(QColor(Qt::lightGray));
    act_fill_ = new IconAction(tr("Fill color"), "psi/select", tr("Fill color"), 0, this, nullptr, true);
    act_fill_->setIcon(QIcon(pixmap));
    act_fill_->setChecked(false);

    act_widths_   = new IconAction(tr("Stroke width"), "psi/drawPaths", tr("Stroke width"), 0, this);
    act_modes_    = new IconAction(tr("Edit mode"), "psi/select", tr("Edit mode"), 0, this);
    group_widths_ = new QActionGroup(this);
    group_modes_  = new QActionGroup(this);

    connect(act_color_, SIGNAL(triggered()), SLOT(setStrokeColor()));
    connect(act_fill_, SIGNAL(triggered(bool)), SLOT(setFillColor(bool)));
    connect(group_widths_, SIGNAL(triggered(QAction *)), SLOT(setStrokeWidth(QAction *)));
    connect(group_modes_, SIGNAL(triggered(QAction *)), SLOT(setMode(QAction *)));
    connect(act_save_, SIGNAL(triggered()), SLOT(save()));
    connect(act_geometry_, SIGNAL(triggered()), SLOT(setGeometry()));
    connect(act_clear_, SIGNAL(triggered()), wbWidget_, SLOT(clear()));

    pixmap = QPixmap(2, 2);
    pixmap.fill(QColor(Qt::black));
    QAction *widthaction = new QAction(QIcon(pixmap), tr("Thin stroke"), group_widths_);
    widthaction->setData(QVariant(1));
    widthaction->setCheckable(true);
    widthaction->trigger();
    pixmap = QPixmap(6, 6);
    pixmap.fill(QColor(Qt::black));
    widthaction = new QAction(QIcon(pixmap), tr("Medium stroke"), group_widths_);
    widthaction->setData(QVariant(3));
    widthaction->setCheckable(true);
    pixmap = QPixmap(12, 12);
    pixmap.fill(QColor(Qt::black));
    widthaction = new QAction(QIcon(pixmap), tr("Thick stroke"), group_widths_);
    widthaction->setData(QVariant(6));
    widthaction->setCheckable(true);

    // Moving, scaling, rotating and panning are gestures (see the controls help), not modes.
    auto addMode = [this](const QString &text, const char *icon, WbWidget::Mode mode, const QString &key) {
        auto action = new IconAction(text, icon, text, 0, group_modes_);
        action->setData(QVariant(int(mode)));
        action->setCheckable(true);
        action->setToolTip(tr("%1 (%2)").arg(text, key));
        return action;
    };
    addMode(tr("Select"), "psi/select", WbWidget::Mode::Select, QStringLiteral("V"));
    addMode(tr("Draw paths"), "psi/drawPaths", WbWidget::Mode::DrawPath, QStringLiteral("P"))->trigger();
    addMode(tr("Draw lines"), "psi/drawLines", WbWidget::Mode::DrawLine, QStringLiteral("L"));
    addMode(tr("Draw rectangles"), "psi/drawRectangles", WbWidget::Mode::DrawRectangle, QStringLiteral("R"));
    addMode(tr("Draw ellipses"), "psi/drawEllipses", WbWidget::Mode::DrawEllipse, QStringLiteral("O"));
    {
        // The iconset has no rhombus.
        QPixmap rhombus(16, 16);
        rhombus.fill(Qt::transparent);
        QPainter painter(&rhombus);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette().color(QPalette::WindowText), 1.5));
        painter.drawPolygon(QPolygonF() << QPointF(8, 1.5) << QPointF(14.5, 8) << QPointF(8, 14.5) << QPointF(1.5, 8));
        painter.end();
        auto action = addMode(tr("Draw rhombuses"), "", WbWidget::Mode::DrawRhombus, QStringLiteral("D"));
        action->setIcon(QIcon(rhombus));
    }
    addMode(tr("Add text"), "psi/addText", WbWidget::Mode::DrawText, QStringLiteral("T"));
    addMode(tr("Add images"), "psi/addImage", WbWidget::Mode::DrawImage, QStringLiteral("I"));
    addMode(tr("Erase"), "psi/erase", WbWidget::Mode::Erase, QStringLiteral("E"));

    connect(wbWidget_, &WbWidget::modeChanged, this, [this](WbWidget::Mode mode) {
        const auto actions = group_modes_->actions();
        for (QAction *action : actions) {
            if (action->isCheckable() && action->data().toInt() == int(mode))
                action->setChecked(true);
        }
    });
    connect(wbWidget_, &WbWidget::strokeWidthChanged, this, [this](int width) {
        const auto actions = group_widths_->actions();
        for (QAction *action : actions) {
            if (action->data().toInt() == width)
                action->setChecked(true);
        }
    });
    connect(wbWidget_, &WbWidget::canvasContextMenuRequested, this,
            [this](const QPoint &globalPos) { pm_settings_->exec(globalPos); });

    act_help_ = new QAction(style()->standardIcon(QStyle::SP_DialogHelpButton), tr("Controls"), this);
    act_help_->setToolTip(tr("Mouse and keyboard controls"));
    act_help_->setShortcut(QKeySequence::HelpContents);
    connect(act_help_, &QAction::triggered, this, &WbDlg::showHelp);

    menu_widths_ = new QMenu(this);
    menu_widths_->addActions(group_widths_->actions());
    act_widths_->setMenu(menu_widths_);

    menu_modes_ = new QMenu(this);
    menu_modes_->addActions(group_modes_->actions());
    act_modes_->setMenu(menu_modes_);

    toolbar_ = new QToolBar(tr("Whiteboard toolbar"), this);
    toolbar_->setIconSize(QSize(16, 16));
    toolbar_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    // Tools are one click away; the rest of the actions follow on the right.
    toolbar_->addActions(group_modes_->actions());
    toolbar_->addWidget(new StretchWidget(this));
    toolbar_->addAction(act_fill_);
    toolbar_->addAction(act_color_);
    QToolButton *bw = new QToolButton;
    bw->setIcon(IconsetFactory::icon("psi/drawPaths").icon());
    bw->setMenu(menu_widths_);
    bw->setPopupMode(QToolButton::InstantPopup);
    toolbar_->addWidget(bw);
    toolbar_->addSeparator();
    toolbar_->addAction(act_save_);
    toolbar_->addAction(act_geometry_);
    toolbar_->addAction(act_clear_);
    toolbar_->addAction(act_help_);
    vb1->addWidget(toolbar_);

    // Context menu
    pm_settings_ = new QMenu(this);
    connect(pm_settings_, SIGNAL(aboutToShow()), SLOT(buildMenu()));

    X11WM_CLASS("whiteboard");

    // set the Jid -> le_jid.
    le_jid_->setText(QString("%1 (session: %2)").arg(peer_.full(), sid_));
    le_jid_->setCursorPosition(0);
    le_jid_->setToolTip(peer_.full());

    // update the widget icon
    setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());

    setWindowOpacity(double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))
                     / 100);

    setGeometryOptionPath(geometryOption);

    connect(application, &XMPP::Jingle::SXE::Application::syncStateChanged, this, &WbDlg::syncStateChanged);
    connect(application, &QObject::destroyed, this,
            [this]() { syncStateChanged(XMPP::Jingle::SXE::Application::SyncState::Offline); });
    syncStateChanged(application->syncState());
}

WbDlg::~WbDlg() { qDebug("destruct WbDlg"); }

XMPP::Jingle::SXE::Application *WbDlg::application() const { return application_; }

Jid WbDlg::peer() const { return peer_; }

void WbDlg::syncStateChanged(XMPP::Jingle::SXE::Application::SyncState state)
{
    using SyncState = XMPP::Jingle::SXE::Application::SyncState;
    // The host edits its document right away; the peer gets it with the state.
    // A joiner waits for that state, and nothing is sent once the session ended.
    const bool ended = !application_ || application_->state() >= XMPP::Jingle::State::Finishing;
    const bool host  = application_ && application_->isHost();
    setAllowEdits(!ended && (host || state == SyncState::Synchronized));
    if (ended)
        peerLeftSession(peer_);
}

bool WbDlg::allowEdits() const { return allowEdits_; }

void WbDlg::setAllowEdits(bool a)
{
    allowEdits_ = a;
    wbWidget_->setEditable(a);
}

void WbDlg::showHelp()
{
    if (helpDialog_) {
        bringToFront(helpDialog_);
        return;
    }

    // Qt reports Command as Control on macOS; the hints use the platform's names.
#ifdef Q_OS_MACOS
    const QString cmd          = QStringLiteral("\u2318");
    const QString alt          = QStringLiteral("\u2325");
    const QString shift        = QStringLiteral("\u21E7");
    const QString contextClick = tr("Right-click or Control-click");
    const QString rightDrag    = tr("Right-drag (two-finger click and drag on a trackpad)");
    const QString pan          = tr("Space + drag, or two-finger scroll");
    const QString zoom         = tr("%1 + scroll, or pinch").arg(cmd);
#else
    const QString cmd          = tr("Ctrl");
    const QString alt          = tr("Alt");
    const QString shift        = tr("Shift");
    const QString contextClick = tr("Right-click");
    const QString rightDrag    = tr("Right-drag");
    const QString pan          = tr("Middle-drag or Space + drag; the wheel scrolls");
    const QString zoom         = tr("%1 + wheel, or pinch on a touchpad").arg(cmd);
#endif
    auto key = [](const char *sequence) {
        return QKeySequence(QString::fromLatin1(sequence)).toString(QKeySequence::NativeText);
    };
    auto keys = [](QKeySequence::StandardKey standard) {
        return QKeySequence(standard).toString(QKeySequence::NativeText);
    };

    const QList<QPair<QString, QList<QPair<QString, QString>>>> sections {
        { tr("Drawing"),
          { { tr("Draw with the current tool"), tr("Left button") },
            { tr("Straight line"), tr("%1 + drag with the pen").arg(shift) },
            { tr("Tools"), tr("V select, P pen, L line, R rectangle, O ellipse, D rhombus, T text, I image, E eraser") },
            { tr("Square, circle"), tr("%1 + drag with the rectangle or ellipse tool").arg(shift) },
            { tr("Stroke width and text size"), QStringLiteral("[  ]") } } },
        { tr("Selection"),
          { { tr("Select and move"), tr("%1 + drag an item (or the Select tool)").arg(cmd) },
            { tr("Select an area"), tr("%1 + drag on the empty canvas").arg(cmd) },
            { tr("Add to or remove from the selection"), tr("%1 + %2 + click").arg(cmd, shift) },
            { tr("Select all / deselect"), tr("%1 / Esc").arg(keys(QKeySequence::SelectAll)) },
            { tr("Move by 1 or 10 units"), tr("Arrow keys / %1 + arrow keys").arg(shift) },
            { tr("Scale"), tr("%1 + %2 + drag the selection, or + and \u2212").arg(cmd, shift) },
            { tr("Rotate"), tr("%1, %2 + wheel, or , and . (15\u00B0), < and > (1\u00B0)").arg(rightDrag, alt) },
            { tr("Duplicate"), key("Ctrl+D") },
            { tr("Delete"), tr("Delete or Backspace") },
            { tr("Group / ungroup"), tr("%1 / %2").arg(key("Ctrl+G"), key("Ctrl+Shift+G")) },
            { tr("Bring forward / send backward"), tr("Page Up / Page Down") },
            { tr("Bring to front / send to back"), tr("Home / End") },
            { tr("Item menu"), contextClick } } },
        { tr("View"),
          { { tr("Zoom"), tr("%1, %2 / %3").arg(zoom, key("Ctrl+="), keys(QKeySequence::ZoomOut)) },
            { tr("Fit the canvas"), key("Ctrl+0") },
            { tr("Pan a zoomed canvas"), pan } } },
        { tr("Images"),
          { { tr("Insert"), tr("Drop image files on the canvas, paste with %1, or click with the image tool")
                                .arg(keys(QKeySequence::Paste)) } } },
    };

    QString html = QStringLiteral("<table cellspacing='0' cellpadding='3'>");
    for (const auto &section : sections) {
        html += QStringLiteral("<tr><td colspan='2'><b>%1</b></td></tr>").arg(section.first.toHtmlEscaped());
        for (const auto &row : section.second)
            html += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>")
                        .arg(row.first.toHtmlEscaped(), row.second.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");

    helpDialog_ = new QDialog(this);
    helpDialog_->setAttribute(Qt::WA_DeleteOnClose);
    helpDialog_->setWindowTitle(tr("Whiteboard Controls"));
    auto layout = new QVBoxLayout(helpDialog_);
    auto label  = new QLabel(html, helpDialog_);
    label->setTextFormat(Qt::RichText);
    layout->addWidget(label);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, helpDialog_);
    connect(buttons, &QDialogButtonBox::rejected, helpDialog_, &QDialog::close);
    layout->addWidget(buttons);
    helpDialog_->show();
}

void WbDlg::peerLeftSession(const Jid &peer) { le_jid_->setText(tr("%1 left (session: %2).").arg(peer.full(), sid_)); }

void WbDlg::endSession()
{
    // Closing the window is the way to leave a whiteboard: terminate the underlying Jingle session
    if (application_ && application_->pad()->session()->state() != XMPP::Jingle::State::Finished)
        application_->pad()->session()->terminate(XMPP::Jingle::Reason::Success);

    emit sessionEnded(this);
}

void WbDlg::activated()
{
    if (pending_ > 0) {
        pending_ = 0;
        updateCaption();
    }
    doFlash(false);
}

void WbDlg::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && !PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool())
        close();
    else if (e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier
             && !PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool())
        close();
    else
        e->ignore();
}

void WbDlg::closeEvent(QCloseEvent *e)
{
    e->accept();
    if (keepOpen_) {
        int n = QMessageBox::information(
            this, tr("Warning"),
            tr("A new whiteboard message was just received.\nDo you still want to close the window?"),
            QMessageBox::Yes | QMessageBox::No);
        if (n != QMessageBox::Yes) {
            e->ignore();
            return;
        }
    }

    endSession();
}

void WbDlg::showEvent(QShowEvent *) { setSelfDestruct(0); }

void WbDlg::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::ActivationChange && isActiveWindow()) {
        activated();
    }
    e->ignore();
}

void WbDlg::setStrokeColor()
{
    QColor newColor = QColorDialog::getColor();
    if (newColor.isValid()) {
        QPixmap pixmap(16, 16);
        pixmap.fill(newColor);
        act_color_->setIcon(QIcon(pixmap));
        wbWidget_->setStrokeColor(newColor);
    }
}

void WbDlg::setFillColor(bool fill)
{
    QColor newColor;
    if (fill) {
        newColor = QColorDialog::getColor();
        if (newColor.isValid()) {
            QPixmap pixmap(16, 16);
            pixmap.fill(newColor);
            act_fill_->setIcon(QIcon(pixmap));
        }
    } else {
        newColor = Qt::transparent;
    }
    wbWidget_->setFillColor(newColor);
}

void WbDlg::setStrokeWidth(QAction *a) { wbWidget_->setStrokeWidth(a->data().toInt()); }

void WbDlg::setMode(QAction *a) { wbWidget_->setMode(WbWidget::Mode(a->data().toInt())); }

void WbDlg::setKeepOpenFalse() { keepOpen_ = false; }

void WbDlg::buildMenu()
{
    pm_settings_->clear();
    pm_settings_->addAction(act_modes_);
    pm_settings_->addAction(act_widths_);
    pm_settings_->addAction(act_color_);
    pm_settings_->addSeparator();
    pm_settings_->addAction(act_save_);
    pm_settings_->addAction(act_clear_);
    pm_settings_->addSeparator();
    pm_settings_->addAction(act_help_);
}

void WbDlg::setGeometry()
{
    // TODO: make a proper dialog
    QSize size;

    bool ok;
    size.setWidth(QInputDialog::getInt(this, tr("Set new width:"), tr("Width:"),
                                       static_cast<int>(wbWidget_->scene()->sceneRect().width()), 10, 100000, 10, &ok));
    if (!ok) {
        return;
    }

    size.setHeight(QInputDialog::getInt(this, tr("Set new height:"), tr("Height:"),
                                        static_cast<int>(wbWidget_->scene()->sceneRect().height()), 10, 100000, 10,
                                        &ok));
    if (!ok)
        return;

    wbWidget_->setSize(size);
}

void WbDlg::save()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Whitebaord"), QDir::homePath(),
                                                    tr("Scalable Vector Graphics (*.svg)"));
    fileName         = fileName.trimmed();
    if (!fileName.endsWith(".svg", Qt::CaseInsensitive))
        fileName += ".svg";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    document_->document().save(stream, 2);

    file.close();
}

void WbDlg::contextMenuEvent(QContextMenuEvent *e)
{
    pm_settings_->exec(QCursor::pos());
    e->accept();
}

void WbDlg::setSelfDestruct(int minutes)
{
    if (minutes <= 0) {
        if (selfDestruct_) {
            delete selfDestruct_;
            selfDestruct_ = nullptr;
        }
        return;
    }

    if (!selfDestruct_) {
        selfDestruct_ = new QTimer(this);
        connect(selfDestruct_, SIGNAL(timeout()), SLOT(endSession()));
    }

    selfDestruct_->start(minutes * 60000);
}

void WbDlg::updateCaption()
{
    QString cap = "";

    if (pending_ > 0) {
        cap += "* ";
        if (pending_ > 1)
            cap += QString("[%1] ").arg(pending_);
    }
    cap += peer_.full();

    setWindowTitle(cap);
}
