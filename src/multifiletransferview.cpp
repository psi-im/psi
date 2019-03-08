#include "multifiletransferview.h"
#include "multifiletransfermodel.h"
#include "psitooltip.h"

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
        itemHeight = fontPixelSize * 3 + 3 * fm.leading(); // 2 inter-line + 2 half-inter-line on top and bottom
        spacing  = fm.leading() / 2;
        iconRect = QRect(spacing, spacing, itemHeight - 2 * spacing, itemHeight - 2 * spacing);
        textLeft = iconRect.right() + spacing;
        textTop  = fm.leading() / 2;
        speedTop = textTop + fontPixelSize + fm.leading();
        progressHeight = fontPixelSize / 3;
        if (progressHeight < 4) {
            progressHeight = 4; // min 4px
        }
        int progressCenter = speedTop + fontPixelSize + fm.leading() + fontPixelSize / 2;
        progressTop = progressCenter - progressHeight / 2;

        // render progressTexture
        QColor darkProgress(40, 40, 80); // need some setting for this or take QPalette::Highlight
        QColor lightProgress(70, 70, 110);
        progressTexture = QPixmap(progressHeight, progressHeight);
        QPainter p(&progressTexture);
        p.fillRect(0, 0, progressHeight, progressHeight, darkProgress);
        p.setBrush(QBrush(lightProgress));
        p.drawPolygon(QPolygon({QPoint(progressHeight / 2, 0), QPoint(progressHeight, 0),
                                QPoint(progressHeight / 2, progressHeight), QPoint(0, progressHeight)}));

    }
    return QSize(option.rect.width(), itemHeight);
}

void MultiFileTransferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto pix = index.data(Qt::DecorationRole).value<QPixmap>();
    if (!pix.isNull()) {
        pix = pix.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    int right = option.rect.right() - spacing;

    // translated coords
    int textLeft = this->textLeft + option.rect.left();
    int textTop  = this->textTop + option.rect.top();
    int speedTop = this->speedTop + option.rect.top();
    int progressTop = this->progressTop + option.rect.top();

    painter->drawPixmap(iconRect.translated(option.rect.topLeft()), pix);
    painter->drawText(QRect(QPoint(textLeft, textTop),QPoint(right, textTop + fontPixelSize)),
                      option.displayAlignment, index.data(Qt::DisplayRole).toString());

    quint64 fullSize = index.data(MultiFileTransferModel::FullSizeRole).toULongLong();
    quint64 curSize = index.data(MultiFileTransferModel::CurrentSizeRole).toULongLong();
    int timeRemaining = index.data(MultiFileTransferModel::TimeRemainingRole).toULongLong();
    auto status = index.data(MultiFileTransferModel::StateRole).toInt();

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

        switch (status) {
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
    painter->drawText(QRect(QPoint(textLeft, speedTop),QPoint(right, speedTop + fontPixelSize)),
                      option.displayAlignment, s);

    // -----------------------------
    // Transfer progress bar
    // -----------------------------
    QBrush b;
    b.setTexture(progressTexture);
    QPainterPath p;
    p.addEllipse(textLeft, progressTop, progressHeight, progressHeight);
    p.addEllipse(right - progressHeight, progressTop, progressHeight, progressHeight);
    p.addRect(textLeft + progressHeight / 2, progressTop, right - progressHeight, progressHeight);
    painter->fillPath(p, b);
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
