/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MESSAGEPREVIEW_H
#define MESSAGEPREVIEW_H
#include <QColor>
#include <QFrame>
#include <QList>
#include <QPoint>
#include <optional>
class QLabel;
class QPushButton;
class ChatMessageText;
class CodeBlockWidget;
class FileSharingDeviceOpener;
class LinkButton;
class QVBoxLayout;

// A selectable context card for replies, links and message references:
// collapsed to a short excerpt until the user asks to see all of it.
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
    // Whether a plain click on the title, the text or the bar emits activated().
    // Clicks that links, selections, media or embedded controls claim never do.
    // The first click of a double click counts too: such text is selected by dragging.
    void setActivatable(bool activatable);
    // Installed on every child, including those later content creates.
    void setChildEventFilter(QObject *filter);
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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void          updatePreview();
    void          scheduleUpdatePreview();
    void          clearParts();
    void          rebuildParts(const QString &html);
    void          addPart(QWidget *part);
    void          filterChild(QWidget *child);
    void          handlePlainClick();
    void          setTitle(const QString &title);
    void          refreshColors();
    QColor        barColor_;
    QColor        textColor_;
    QLabel       *title_ = nullptr;
    QFrame       *bar_ = nullptr;
    QVBoxLayout  *column_ = nullptr;
    QWidget      *footer_ = nullptr;
    // clip_ shows the top of content_; Qt clips children to their parent.
    QWidget      *clip_ = nullptr;
    QWidget      *content_ = nullptr;
    QVBoxLayout  *contentLayout_ = nullptr;
    ChatMessageText *text_ = nullptr; // Always the first item of contentLayout_.
    QList<ChatMessageText *> extraProse_;
    QList<CodeBlockWidget *> codeBlocks_;
    QObject      *childFilter_ = nullptr;
    QString          richText_; // Null for plain content.
    bool             updateScheduled_ = false;
    LinkButton   *toggle_ = nullptr;
    QPushButton  *close_ = nullptr;
    bool          expanded_ = false;
    bool          activatable_ = false;
    std::optional<QPoint> surfacePress_;
};
#endif
