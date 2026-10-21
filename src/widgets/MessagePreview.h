/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MESSAGEPREVIEW_H
#define MESSAGEPREVIEW_H
#include <QColor>
#include <QFrame>
class QLabel;
class QPushButton;
class QTextBrowser;
class LinkButton;
class QVBoxLayout;

// A selectable, bounded context card for replies, links and message references.
class MessagePreview : public QFrame {
    Q_OBJECT
public:
    explicit MessagePreview(const QColor &barColor, const QColor &textColor = {}, QWidget *parent = nullptr);
    // An invalid text color restores normal Qt palette inheritance.
    void setColors(const QColor &barColor, const QColor &textColor = {});
    void setLinkColor(const QColor &color);
    void setContent(const QString &title, const QString &text);
    void setClosable(bool);
    // Optional controls below the preview text, owned by the preview.
    void setFooter(QWidget *widget);
    bool isExpanded() const { return expanded_; }
signals:
    void dismissed();
    void heightChanged();

protected:
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void          updatePreview();
    void          refreshColors();
    QColor        barColor_;
    QColor        textColor_;
    QLabel       *title_ = nullptr;
    QFrame       *bar_ = nullptr;
    QVBoxLayout  *column_ = nullptr;
    QWidget      *footer_ = nullptr;
    QTextBrowser *text_ = nullptr;
    LinkButton   *toggle_ = nullptr;
    QPushButton  *close_ = nullptr;
    bool          expanded_ = false;
};
#endif
