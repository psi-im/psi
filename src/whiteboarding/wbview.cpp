/*
 * wbview.cpp - whiteboard canvas with its toolbar
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

#include "wbview.h"

#include "common.h"
#include "iconaction.h"
#include "iconset.h"
#include "stretchwidget.h"

#include <QActionGroup>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QStyle>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

WbView::WbView(XMPP::Jingle::SXE::Document *document, QWidget *parent) : QWidget(parent), document_(document)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    wbWidget_ = new WbWidget(document, this);
    wbWidget_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    layout->addWidget(wbWidget_);

    act_save_ = new IconAction(tr("Save session"), "psi/saveBoard", tr("Save the contents of the whiteboard"), 0, this);
    act_geometry_ = new IconAction(tr("Change the geometry"), "psi/whiteboard", tr("Change the geometry"), 0, this);
    act_clear_
        = new IconAction(tr("Clear the whiteboard for everyone"), "psi/clearChat", tr("Clear the whiteboard"), 0, this);

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

    act_import_ = new IconAction(tr("Import SVG"), "psi/import", tr("Add the drawing of an SVG file"), 0, this);

    connect(act_color_, &QAction::triggered, this, &WbView::setStrokeColor);
    connect(act_fill_, &QAction::triggered, this, &WbView::setFillColor);
    connect(group_widths_, &QActionGroup::triggered, this,
            [this](QAction *action) { wbWidget_->setStrokeWidth(action->data().toInt()); });
    connect(group_modes_, &QActionGroup::triggered, this,
            [this](QAction *action) { wbWidget_->setMode(WbWidget::Mode(action->data().toInt())); });
    connect(act_save_, &QAction::triggered, this, &WbView::save);
    connect(act_import_, &QAction::triggered, this, &WbView::importSvg);
    connect(act_geometry_, &QAction::triggered, this, &WbView::setCanvasSize);
    connect(act_clear_, &QAction::triggered, wbWidget_, &WbWidget::clear);

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
            [this](const QPoint &globalPos) { menu_->exec(globalPos); });

    act_help_ = new QAction(style()->standardIcon(QStyle::SP_DialogHelpButton), tr("Controls"), this);
    act_help_->setToolTip(tr("Mouse and keyboard controls"));
    act_help_->setShortcut(QKeySequence::HelpContents);
    connect(act_help_, &QAction::triggered, this, &WbView::showHelp);

    auto menuWidths = new QMenu(this);
    menuWidths->addActions(group_widths_->actions());
    act_widths_->setMenu(menuWidths);

    auto menuModes = new QMenu(this);
    menuModes->addActions(group_modes_->actions());
    act_modes_->setMenu(menuModes);

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
    bw->setMenu(menuWidths);
    bw->setPopupMode(QToolButton::InstantPopup);
    toolbar_->addWidget(bw);
    toolbar_->addSeparator();
    toolbar_->addAction(act_save_);
    toolbar_->addAction(act_import_);
    toolbar_->addAction(act_geometry_);
    toolbar_->addAction(act_clear_);
    toolbar_->addAction(act_help_);
    layout->addWidget(toolbar_);

    menu_ = new QMenu(this);
    connect(menu_, &QMenu::aboutToShow, this, &WbView::buildMenu);
}

XMPP::Jingle::SXE::Document *WbView::document() const { return document_; }

void WbView::setEditable(bool editable)
{
    editable_ = editable;
    updateTools();
}

bool WbView::isEditable() const { return editable_; }

void WbView::setPreview(bool preview)
{
    preview_ = preview;
    toolbar_->setVisible(!preview);
    wbWidget_->setFocusPolicy(preview ? Qt::NoFocus : Qt::StrongFocus);
    if (preview)
        wbWidget_->fitToView();
    updateTools();
}

void WbView::updateTools()
{
    const bool editable = editable_ && !preview_;
    wbWidget_->setEditable(editable);
    group_modes_->setEnabled(editable);
    act_clear_->setEnabled(editable);
    act_import_->setEnabled(editable);
    act_geometry_->setEnabled(editable);
}

void WbView::showHelp()
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
    auto keys
        = [](QKeySequence::StandardKey standard) { return QKeySequence(standard).toString(QKeySequence::NativeText); };

    const QList<QPair<QString, QList<QPair<QString, QString>>>> sections {
        { tr("Drawing"),
          { { tr("Draw with the current tool"), tr("Left button") },
            { tr("Straight line"), tr("%1 + drag with the pen").arg(shift) },
            { tr("Tools"),
              tr("V select, P pen, L line, R rectangle, O ellipse, D rhombus, T text, I image, E eraser") },
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
          { { tr("Insert"),
              tr("Drop image files on the canvas, paste with %1, or click with the image tool")
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

void WbView::setStrokeColor()
{
    QColor newColor = QColorDialog::getColor();
    if (newColor.isValid()) {
        QPixmap pixmap(16, 16);
        pixmap.fill(newColor);
        act_color_->setIcon(QIcon(pixmap));
        wbWidget_->setStrokeColor(newColor);
    }
}

void WbView::setFillColor(bool fill)
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

void WbView::buildMenu()
{
    menu_->clear();
    menu_->addAction(act_modes_);
    menu_->addAction(act_widths_);
    menu_->addAction(act_color_);
    menu_->addSeparator();
    menu_->addAction(act_save_);
    menu_->addAction(act_import_);
    menu_->addAction(act_clear_);
    menu_->addSeparator();
    menu_->addAction(act_help_);
}

void WbView::setCanvasSize()
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

void WbView::save()
{
    if (!document_)
        return;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Whiteboard"), QDir::homePath(),
                                                    tr("Scalable Vector Graphics (*.svg)"));
    fileName         = fileName.trimmed();
    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".svg", Qt::CaseInsensitive))
        fileName += ".svg";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    document_->document().save(stream, 2);
}

void WbView::importSvg()
{
    if (!document_ || !editable_)
        return;
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import SVG"), QDir::homePath(),
                                                          tr("Scalable Vector Graphics (*.svg)"));
    QFile         file(fileName);
    if (fileName.isEmpty() || !file.open(QIODevice::ReadOnly))
        return;
    QDomDocument svg;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    const bool parsed = svg.setContent(&file, true);
#else
    const bool parsed = bool(svg.setContent(&file, QDomDocument::ParseOption::UseNamespaceProcessing));
#endif
    if (!parsed || svg.documentElement().localName() != QLatin1String("svg")) {
        QMessageBox::warning(this, tr("Import SVG"), tr("The file is not an SVG document."));
        return;
    }
    // The elements are added on top of the current drawing; the canvas keeps its size.
    const auto root = document_->document().documentElement();
    for (auto child = svg.documentElement().firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        document_->insertNodeAfter(child, root);
    document_->flush();
}

void WbView::contextMenuEvent(QContextMenuEvent *event)
{
    if (preview_)
        return;
    menu_->exec(event->globalPos());
    event->accept();
}
