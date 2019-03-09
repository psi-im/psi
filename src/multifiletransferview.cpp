/*
 * multifiletransferview.h - file transfer delegate
 * Copyright (C) 2019 Sergey Ilinykh
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "multifiletransferview.h"
#include "multifiletransfermodel.h"
#include "psitooltip.h"
#include "iconset.h"

#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTime>
#include <QAbstractItemView>

void MultiFileTransferDelegate::niceUnit(qlonglong n, qlonglong *div, QString *unit)
{
    qlonglong gb = 1024 * 1024 * 1024;
    qlonglong mb = 1024 * 1024;
    qlonglong kb = 1024;
    if(n >= gb) {
        *div = gb;
        *unit = QString("GB");
    }
    else if(n >= mb) {
        *div = mb;
        *unit = QString("MB");
    }
    else if(n >= kb) {
        *div = kb;
        *unit = QString("KB");
    }
    else {
        *div = 1;
        *unit = QString("B");
    }
}

QString MultiFileTransferDelegate::roundedNumber(qlonglong n, qlonglong div)
{
    bool decimal = false;
    if(div >= 1024) {
        div /= 10;
        decimal = true;
    }
    qlonglong x_long = n / div;
    int x = (int)x_long;
    if(decimal) {
        double f = (double)x;
        f /= 10;
        return QString::number(f, 'f', 1);
    }
    else
        return QString::number(x);
}

QSize MultiFileTransferDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    QFontMetrics fm(option.font);
    int newFontSize = fm.height();
    if (fontPixelSize != newFontSize) {
        // we need to recompute all relative geometry based on new
        fontPixelSize = newFontSize;
        spacing = fm.leading();
        if (spacing < 2) {
            spacing = 2;
        }
        itemHeight = fontPixelSize * 3 + 2 * fm.leading() + 2 * spacing; // 2 inter-line + 2 half-inter-line on top and bottom
        addButtonHeight = fontPixelSize * 2 + fm.leading() + 2 * spacing; // make its height as two lines of text + spacing
        iconRect = QRect(spacing, spacing, itemHeight - 2 * spacing, itemHeight - 2 * spacing);
        textLeft = iconRect.right() + spacing;
        textTop  = spacing;
        speedTop = textTop + fm.lineSpacing();
        progressHeight = fontPixelSize / 3;
        if (progressHeight < 10) {
            progressHeight = 10; // min 4px
        }
        int progressCenter = speedTop + fm.lineSpacing() + fontPixelSize / 2;
        progressTop = progressCenter - progressHeight / 2;

        // render progressTexture
        QColor darkProgress(100, 100, 200); // need some setting for this or take QPalette::Highlight
        QColor lightProgress(160, 160, 255);
        progressTexture = QPixmap(progressHeight, progressHeight);
        QPainter p(&progressTexture);
        p.setPen(Qt::NoPen);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(0, 0, progressHeight, progressHeight, darkProgress);
        p.setBrush(QBrush(lightProgress));
        p.drawPolygon(QPolygon({QPoint(progressHeight / 2, 0), QPoint(progressHeight, 0),
                                QPoint(progressHeight / 2, progressHeight), QPoint(0, progressHeight)}),Qt::WindingFill);

    }

    if (index.data(MultiFileTransferModel::StateRole) == MultiFileTransferModel::AddTemplate) {
        return QSize(addButtonHeight, addButtonHeight);
    }
    return QSize(itemHeight, itemHeight);
}

void MultiFileTransferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto state = index.data(MultiFileTransferModel::StateRole).toInt();

    if (state == MultiFileTransferModel::AddTemplate) {
        QRect btnRect(spacing, spacing, option.rect.width() - 2 * spacing, option.rect.height() - 2 * spacing);
        painter->save();
        QColor rc = option.palette.color(QPalette::WindowText);
        rc.setAlpha(128);
        QPen pen(rc);
        pen.setWidth(spacing >= 2? spacing / 2 : 1);
        painter->setPen(rc);
        painter->drawRoundedRect(btnRect.translated(option.rect.topLeft()), spacing * 2, spacing * 2);

        QRect ir(0, 0, option.rect.height() - 4 * spacing, option.rect.height() - 4 * spacing);
        ir.moveCenter(option.rect.center());
        painter->drawPixmap(ir, IconsetFactory::icon("psi/add").pixmap());
        painter->restore();
        return;
    }

    // translated coords
    int textLeft = this->textLeft + option.rect.left();
    int textTop  = this->textTop + option.rect.top();
    int speedTop = this->speedTop + option.rect.top();
    int progressTop = this->progressTop + option.rect.top();
    int right = option.rect.right() - spacing;

    // draw file icon
    auto icon = index.data(Qt::DecorationRole).value<QIcon>();
    painter->drawPixmap(iconRect.translated(option.rect.topLeft()), icon.pixmap(iconRect.size()));

    // draw file name
    painter->save();
    auto f = option.font;
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(QRect(QPoint(textLeft, textTop),QPoint(right, textTop + fontPixelSize)),
                      option.displayAlignment, index.data(Qt::DisplayRole).toString());
    painter->restore();

    // generate and draw status line
    quint64 fullSize = index.data(MultiFileTransferModel::FullSizeRole).toULongLong();
    quint64 curSize = index.data(MultiFileTransferModel::CurrentSizeRole).toULongLong();
    int timeRemaining = index.data(MultiFileTransferModel::TimeRemainingRole).toULongLong();

    // -----------------------------
    // Transfer current status line
    // -----------------------------
    QString s;
    {
        qlonglong div;
        QString unit;
        niceUnit(fullSize, &div, &unit);

        s = roundedNumber(curSize, div) + '/' + roundedNumber(fullSize, div) + unit;
        QString space(" ");

        switch (state) {
        case MultiFileTransferModel::Pending:
            s += (space + tr("[Pending]"));
            break;
        case MultiFileTransferModel::Active:
        {
            int speed = index.data(MultiFileTransferModel::SpeedRole).toInt();

            if(speed == 0)
                s += QString(" ") + tr("[Stalled]");
            else {
                niceUnit(speed, &div, &unit);
                s += QString(" @ ") + tr("%1%2/s").arg(roundedNumber(speed, div)).arg(unit);

                s += ", ";
                QTime t = QTime().addSecs(timeRemaining);
                s += tr("%1h%2m%3s remaining").arg(t.hour()).arg(t.minute()).arg(t.second());
            }
            break;
        }
        case MultiFileTransferModel::Failed:
            s += (space + tr("[Failed]") + index.data(MultiFileTransferModel::ErrorStringRole).toString());
            break;
        case MultiFileTransferModel::Done:
            s += (space + tr("[Done]"));
            break;
        }
    }
    painter->save();
    auto spc = painter->pen().color();
    spc.setAlpha(128);
    painter->setPen(spc);
    QFontMetrics fm(option.font);
    auto str = QRect(QPoint(textLeft, speedTop),QPoint(right, speedTop + fontPixelSize));
    auto stbr = fm.boundingRect(s);
    stbr.moveCenter(str.center());

    painter->drawText(stbr, option.displayAlignment, s);
    painter->restore();

    // -----------------------------
    // Transfer progress bar
    // -----------------------------
    painter->save();
    painter->setPen(Qt::NoPen);
    QBrush b;
    b.setTexture(progressTexture);
    QPainterPath p;
    p.setFillRule(Qt::WindingFill);
    p.addEllipse(textLeft, progressTop, progressHeight, progressHeight);
    p.addEllipse(right - progressHeight, progressTop, progressHeight, progressHeight);
    p.addRect(QRect(QPoint(textLeft + progressHeight / 2, progressTop),
                    QPoint(right - progressHeight / 2 - 1, progressTop + progressHeight - 1)));
    painter->fillPath(p, b);
    painter->restore();
}

bool MultiFileTransferDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(option);
    if (index.isValid() && event->type() == QEvent::ToolTip) {
        QString tip = index.data(Qt::ToolTipRole).toString();
        PsiToolTip::showText(event->pos(), tip, view);
        return true;
    }
    return false;
}

bool MultiFileTransferDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(option);
    QMouseEvent *me;
    if (index.isValid() && event->type() == QEvent::MouseButtonPress &&
            (me = static_cast<QMouseEvent *>(event))->button() == Qt::RightButton) {
        // seems like we need context menu
        QMenu *menu = new QMenu;
        menu->setAttribute(Qt::WA_DeleteOnClose);
        auto status = model->data(index, MultiFileTransferModel::StateRole).toInt();
        if (status == MultiFileTransferModel::Pending || status == MultiFileTransferModel::Active) {
            connect(menu->addAction(tr("Reject")), &QAction::triggered, this, [model, index](){
                model->setData(index, 1, MultiFileTransferModel::RejectFileRole);
            });
        }
        return true;
    }
    return false;
}
