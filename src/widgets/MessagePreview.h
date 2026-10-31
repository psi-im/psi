/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MESSAGEPREVIEW_H
#define MESSAGEPREVIEW_H
#include <QColor>
#include <QFrame>
#include <QPoint>
class QLabel;
class QPushButton;
class ChatMessageText;
class FileSharingDeviceOpener;
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
    // Rich text as in a message: links, emoticons and media, with the
    // preview's text colour. Optionally without lines starting with '>'.
    void setRichContent(const QString &title, const QString &html, bool withoutQuotedLines = false);
    void setMediaOpener(FileSharingDeviceOpener *opener);
    void setClosable(bool);
    // Reply previews can navigate to their referenced message. Other preview
    // uses (attachments, links, composer) remain non-navigable.
    void setNavigationEnabled(bool enabled);
    bool navigationEnabled() const { return navigationEnabled_; }
    // Optional controls below the preview text, owned by the preview.
    void setFooter(QWidget *widget);
    bool isExpanded() const { return expanded_; }
    // Width of the title, text and footer laid out without wrapping.
    int naturalWidth() const;
signals:
    void dismissed();
    void heightChanged();
    void activated();

protected:
    bool eventFilter(QObject *target, QEvent *event) override;
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void          updatePreview();
    void          setTitle(const QString &title);
    void          refreshColors();
    QColor        barColor_;
    QColor        textColor_;
    QLabel       *title_ = nullptr;
    QFrame       *bar_ = nullptr;
    QVBoxLayout  *column_ = nullptr;
    QWidget      *footer_ = nullptr;
    ChatMessageText *text_ = nullptr;
    QString          richText_; // Null for plain content.
    bool             updateScheduled_ = false;
    LinkButton   *toggle_ = nullptr;
    QPushButton  *close_ = nullptr;
    bool          expanded_ = false;
    bool          navigationEnabled_ = false;
    QObject      *pressedTarget_ = nullptr;
    QPoint        pressedPosition_;
};
#endif
