#include "TrayBadgePainter.h"
#include "TrayBadgePolicy.h"

#include <QImage>
#include <QPixmap>
#include <QtTest>

using namespace Mattermost;

class TrayBadgePainterTest : public QObject {
    Q_OBJECT
private slots:
    void badgePriority()
    {
        QCOMPARE(trayBadgeKind(0, false), TrayBadgeKind::None);
        QCOMPARE(trayBadgeKind(0, true), TrayBadgeKind::ChannelUnread);
        QCOMPARE(trayBadgeKind(2, false), TrayBadgeKind::Attention);
        QCOMPARE(trayBadgeKind(2, true), TrayBadgeKind::Attention);
    }

    void drawsExclusiveColorAndTransparentHalo()
    {
        for (int side : { 16, 22, 32, 64 }) {
            for (auto kind : { TrayBadgeKind::ChannelUnread, TrayBadgeKind::Attention }) {
                // Deliberately use RGB to exercise alpha conversion as well.
                QImage base(side, side, QImage::Format_RGB32);
                base.fill(Qt::white);
                QPixmap icon = QPixmap::fromImage(base);
                TrayBadgePainter::paint(icon, kind, 12);
                const QImage img = icon.toImage().convertToFormat(QImage::Format_ARGB32);
                bool hasColoredBadge = false;
                bool hasHalo = false;
                bool hasOtherColor = false;
                for (int y = 0; y < img.height(); ++y) {
                    for (int x = 0; x < img.width(); ++x) {
                        const QColor pixel = img.pixelColor(x, y);
                        if (pixel.alpha() < 128)
                            hasHalo = true;
                        if (pixel.red() > 160 && pixel.green() < 100 && pixel.blue() < 140) {
                            if (kind == TrayBadgeKind::Attention)
                                hasColoredBadge = true;
                            else
                                hasOtherColor = true;
                        }
                        if (pixel.blue() > 150 && pixel.red() < 130 && pixel.green() > 90) {
                            if (kind == TrayBadgeKind::ChannelUnread)
                                hasColoredBadge = true;
                            else
                                hasOtherColor = true;
                        }
                    }
                }
                QVERIFY2(hasHalo, qPrintable(QStringLiteral("No alpha halo at %1px (kind %2)")
                    .arg(side).arg(static_cast<int>(kind))));
                QVERIFY(hasColoredBadge);
                QVERIFY(!hasOtherColor);
            }
        }
    }

    void leavesBaseUnchangedWithoutBadge()
    {
        QPixmap icon(32, 32);
        icon.fill(Qt::white);
        const auto before = icon.toImage();
        TrayBadgePainter::paint(icon, TrayBadgeKind::None, 0);
        QCOMPARE(icon.toImage(), before);
    }
};

QTEST_MAIN(TrayBadgePainterTest)
#include "TrayBadgePainterTest.moc"
