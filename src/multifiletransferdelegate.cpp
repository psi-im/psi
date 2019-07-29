/*
 * multifiletransferdelegate.cpp - file transfer delegate
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2019  Sergey Ilinykh
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

#include "multifiletransferdelegate.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>

#include "iconset.h"
#include "multifiletransfermodel.h"
#include "psitooltip.h"
#include "textutil.h"

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
    s.reserve(128);
    {
        qlonglong div;
        QString unit = TextUtil::sizeUnit(fullSize, &div);

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
                unit = TextUtil::sizeUnit(speed, &div);
                s += QString(" @ ") + tr("%1%2/s").arg(roundedNumber(speed, div), unit);

                s += ", ";

                QString ts;
                ts.reserve(16);
                int days = 0;
                int hours = 0;
                int minutes = 0;
                if (timeRemaining > 24*3600) {
                    days = timeRemaining / (24*3600);
                    timeRemaining -= (24*3600)*days;
                    ts += tr("%1d").arg(days);
                }
                if (timeRemaining > 3600) {
                    hours = timeRemaining / 3600;
                    timeRemaining -= 3600*hours;
                    if (ts.size() || hours) {
                        ts += tr("%1h").arg(hours);
                    }
                }
                if (timeRemaining > 60) {
                    minutes = timeRemaining / 60;
                    timeRemaining -= 60*minutes;
                    if (ts.size() || minutes) {
                        ts += tr("%1m").arg(minutes);
                    }
                }
                ts += tr("%1s").arg(timeRemaining);
                s += tr("%1 remaining").arg(ts);
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
    const int progressLeft = textLeft + progressHeight / 2;
    const int progressMaxRight = right - progressHeight / 2 - 1;

    QImage ppImg(progressMaxRight - progressLeft + progressHeight, progressHeight, QImage::Format_ARGB32_Premultiplied);
    ppImg.fill(Qt::transparent);
    QPainter pp(&ppImg);
    QPen pen(Qt::NoPen);
    pen.setWidth(0);
    pp.setPen(pen);

    QPainterPath ppath;
    ppath.setFillRule(Qt::WindingFill);
    ppath.addEllipse(0, 0, progressHeight, progressHeight);
    ppath.addRect(QRect(QPoint(progressHeight / 2, 0), QPoint(ppImg.width() - progressHeight / 2, progressHeight)));
    ppath.addEllipse(ppImg.width() - progressHeight, 0, progressHeight, progressHeight);

    // Progress bar background
    pp.fillPath(ppath, QBrush(Qt::lightGray, Qt::SolidPattern));
    // transferred
    int progressWidth = static_cast<int>(ppImg.width() * (curSize / double(fullSize))+0.5);
    if (progressWidth) {
        pp.setClipRect(0, 0, progressWidth, progressHeight);
        pp.fillPath(ppath, QBrush(progressTexture));
    }
    painter->drawImage(textLeft, progressTop, ppImg);
}

bool MultiFileTransferDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(option);
    if (index.isValid() && event->type() == QEvent::ToolTip) {
        QString tip = index.data(Qt::ToolTipRole).toString();
        PsiToolTip::showText(view->mapToGlobal(event->pos()), tip, view);
        return true;
    }
    return false;
}

QWidget *MultiFileTransferDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    auto state = index.data(MultiFileTransferModel::StateRole).toInt();

    if (state == MultiFileTransferModel::AddTemplate) {
        return nullptr;
    }
    QWidget *w = new QWidget(parent);
    w->setFocusPolicy(Qt::StrongFocus);
    w->setAutoFillBackground(true);
    w->setLayout(new QVBoxLayout);
    w->layout()->setSpacing(0);
    w->layout()->addWidget(new QLabel(QString("<b>%1 %2</b>").arg(tr("Description for"), index.data().toString()), w));
    auto te = new QLineEdit(w);
    w->setFocusProxy(te);
    te->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    w->layout()->addWidget(te);
    return w;
}

void MultiFileTransferDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    QRect r(option.rect);
    r.setLeft(textLeft);
    editor->setGeometry(r);
}

void MultiFileTransferDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto te = editor->findChild<QLineEdit*>();
    te->setText(index.data(MultiFileTransferModel::DescriptionRole).toString());
}

void MultiFileTransferDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto te = editor->findChild<QLineEdit*>();
    model->setData(index, te->text(), MultiFileTransferModel::DescriptionRole);
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
        if (status == MultiFileTransferModel::Done) {
            connect(menu->addAction(tr("Open Destination Folder")), &QAction::triggered, this, [model, index](){
                model->setData(index, 1, MultiFileTransferModel::OpenDirRole);
            });
        }
        menu->popup(me->globalPos());
        return true;
    }
    return false;
}
