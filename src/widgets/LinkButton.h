/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LINKBUTTON_H
#define LINKBUTTON_H

#include <QAbstractButton>
#include <QColor>

// Button input/accessibility with text-only rendering, independent of native
// push-button margins, backgrounds and hover animations. The caller owns colors.
class LinkButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor)
public:
    explicit LinkButton(QWidget *parent = nullptr);
    QColor linkColor() const;
    void   setLinkColor(const QColor &color);
    QSize  sizeHint() const override;
    QSize  minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void changeEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;

private:
    QColor color_;
    bool   hovered_ = false;
};
#endif
