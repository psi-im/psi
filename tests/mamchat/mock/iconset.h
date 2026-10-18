#pragma once
#include <QPixmap>
#include <QString>

// Asset boundary only: the tests compile the real PsiRichText parser,
// QTextObjectInterface and painter, without application iconset discovery.
class PsiIcon {
public:
    QSize size() const { return QSize(16, 16); }
    bool  isScalable() const { return true; }
};
class IconsetFactory {
public:
    static const PsiIcon *iconPtr(const QString &name)
    {
        static PsiIcon smile;
        return name == QLatin1String("test/smile") ? &smile : nullptr;
    }
    static QPixmap iconPixmap(const QString &, const QSize &size)
    {
        QPixmap pixmap(size);
        pixmap.fill(QColor(245, 10, 220));
        return pixmap;
    }
};
