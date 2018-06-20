#ifndef PIXMAPRATIOLABEL_H
#define PIXMAPRATIOLABEL_H

#include <QLabel>
#include <QPixmap>
#include <QSize>

class PixmapRatioLabel : public QLabel
{
    Q_OBJECT

public:
    enum class Policy : char {
        FitVertical,
        FitHorizontal,
        FitBoth
    };

    explicit PixmapRatioLabel(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pix);
    void setMaxPixmapSize(const QSize &size);
    inline void setResizePolicy(enum Policy policy) { _policy = policy; }
    inline QSize scaledSize() const { return _scaledPix.size(); }

protected:
    void paintEvent(QPaintEvent *event);

signals:

public slots:

private:
    QPixmap _origPix;
    QPixmap _scaledPix;
    QSize _maxPixSize;
    Policy _policy = Policy::FitBoth;
};

#endif // PIXMAPRATIOLABEL_H
