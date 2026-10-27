/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamChatWidget.h"
#include "widgets/MamImageViewer.h"
#include "widgets/ChatMessageText.h"
#include "widgets/MessagePreview.h"
#include "widgets/LinkButton.h"
#include "widgets/NativeChatRenderers.h"
#include <QContextMenuEvent>
#include <QCursor>
#include <QTextBrowser>
#include <QVBoxLayout>
#include "widgets/urlobject.h"
#include "desktopcolorpalette.h"
#include "filesharingmanager.h"
#include "qiteaudio.h"
#include "qitevideo.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTextBlock>
#include <QTextFragment>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QWheelEvent>
#include <QWindow>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif
#include <QtTest/QtTest>
#include <QtMath>
#include <cmath>
#include <memory>
#include <tuple>

namespace {
void enterWidget(QWidget *widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF local(1, 1);
    QEnterEvent enter(local, local, widget->mapToGlobal(QPoint(1, 1)));
#else
    QEvent enter(QEvent::Enter);
#endif
    QApplication::sendEvent(widget, &enter);
}

MessageView message(const QString &id, const QString &body, int seconds = 0, bool local = false)
{
    MessageView view(MessageView::Message);
    view.setMessageId(id);
    view.setPlainText(body);
    view.setNick(local ? QStringLiteral("Me") : QStringLiteral("Peer"));
    view.setLocal(local);
    view.setDateTime(QDateTime::fromString(QStringLiteral("2026-10-18T10:00:00Z"),
                                           Qt::ISODate).addSecs(seconds));
    return view;
}
}

// A plugin-style renderer: its own frame, the body above the nickname, no
// timestamp and no avatar.
class FramedRenderer final : public NativeChatMessageRenderer {
public:
    mutable int restyled = 0;

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override
    {
        NativeChatPartOptions options;
        options.avatarSize   = 24;
        options.timeFormat   = QStringLiteral("HH:mm");
        options.authorSuffix = QStringLiteral(" says");
        options.boldAuthor   = false;
        return options;
    }

    void arrange(NativeChatMessageCanvas &canvas, const NativeChatMessageInfo &info) const override
    {
        auto *outer = new QVBoxLayout(canvas.row());
        outer->setContentsMargins(4, 4, 4, 4);
        if (auto *selection = canvas.take(NativeChatMessageCanvas::Selection))
            outer->addWidget(selection);
        auto *frame = new QFrame(canvas.row());
        frame->setObjectName(QStringLiteral("pluginFrame"));
        frame->setMaximumWidth(260);
        auto *column = new QVBoxLayout(frame);
        column->addWidget(canvas.take(NativeChatMessageCanvas::Body));
        if (auto *author = canvas.take(NativeChatMessageCanvas::Author))
            column->addWidget(author);
        outer->addWidget(frame, 0, Qt::AlignLeft);
        canvas.setActionAnchor(frame);
        if (info.local)
            canvas.setBodyTextColor(QColor(200, 0, 0));
    }

    void restyle(QWidget *row, const NativeChatMessageInfo &) const override
    {
        ++restyled;
        row->setProperty("restyled", restyled);
    }
};

class MamChatWidgetTest final : public QObject {
    Q_OBJECT
private slots:
    void chatTimelineHasNoFrame()
    {
        MamChatWidget chat;
        QCOMPARE(chat.frameShape(), QFrame::NoFrame);
        QCOMPARE(chat.frameWidth(), 0);
    }

    void nativeMessageMenuCopiesQuotesSelectsAndForwards()
    {
        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setHistoryLoading(true);
        auto original = message("client", "First <b>message</b>");
        original.setReactionTargetId("origin");
        original.setUserId("peer@example.org/phone");
        chat.appendMessage(original);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *row = chat.itemWidget(0);
        auto *body = row->findChild<ChatMessageText *>();
        QVERIFY(body);
        const auto openMenu = [&]() -> QMenu * {
            QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(3, 3), row->mapToGlobal(QPoint(3, 3)));
            QApplication::sendEvent(row, &event);
            return chat.findChild<QMenu *>("mamMessageContextMenu");
        };
        chat.selectMessageRange(0, 0);
        chat.copySelectedMessages();
        const auto fullCopy = QApplication::clipboard()->text();
        chat.cancelMessageSelection();
        auto *menu = openMenu();
        QVERIFY(menu);
        menu->findChild<QAction *>("mamCopyMessage")->trigger();
        QCOMPARE(QApplication::clipboard()->text(), fullCopy);
        delete menu;

        auto cursor = body->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(5, QTextCursor::KeepAnchor);
        body->setTextCursor(cursor);
        menu = openMenu();
        QVERIFY(menu);
        menu->findChild<QAction *>("mamCopyMessage")->trigger();
        QCOMPARE(QApplication::clipboard()->text(), QString("First"));
        QSignalSpy quote(&chat, &MamChatWidget::replyRequested);
        menu->findChild<QAction *>("mamQuoteMessage")->trigger();
        QCOMPARE(quote.size(), 1);
        const auto reply = qvariant_cast<ChatReply>(quote.first().first());
        QCOMPARE(reply.id, QString("origin"));
        QCOMPARE(reply.to, QString("peer@example.org/phone"));
        QCOMPARE(reply.text, QString("First message"));
        QSignalSpy forwarded(&chat, &MamChatWidget::forwardMessageRequested);
        menu->findChild<QAction *>("mamForwardMessage")->trigger();
        QCOMPARE(forwarded.size(), 1);
        QCOMPARE(forwarded.first().at(1).toString(), reply.to);
        QCOMPARE(forwarded.first().at(2).toString(), reply.text);
        menu->findChild<QAction *>("mamSelectMessage")->trigger();
        QCOMPARE(chat.selectedMessageCount(), 1);
        delete menu;
    }

    void imageContextMenuCopiesOnlyHttpLinks_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("expected");
        QTest::newRow("https") << QStringLiteral("https://cdn.example.org/photo.png?key=123")
                               << QStringLiteral("https://cdn.example.org/photo.png?key=123");
        QTest::newRow("http") << QStringLiteral("http://cdn.example.org/photo.png")
                              << QStringLiteral("http://cdn.example.org/photo.png");
        QTest::newRow("file") << QStringLiteral("file:///tmp/photo.png") << QString();
        QTest::newRow("share") << QStringLiteral("share:private-id") << QString();
        QTest::newRow("mailto") << QStringLiteral("mailto:other@example.org") << QString();
        QTest::newRow("http-without-host") << QStringLiteral("http:/photo.png") << QString();
    }

    void imageContextMenuCopiesOnlyHttpLinks()
    {
        QFETCH(QString, source);
        QFETCH(QString, expected);

        FileSharingDeviceOpener opener;
        FileSharingDeviceOpener account;
        opener.accounts.insert(QStringLiteral("account-a"), &account);
        FileSharingItem image(QStringLiteral("image/png"));
        image.source_ = QUrl(source);
        image.preview_ = QImage(64, 48, QImage::Format_RGB32);
        image.preview_.fill(Qt::green);
        account.items.insert(QStringLiteral("picture"), &image);

        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setMediaOpener(&opener);
        chat.setHistoryLoading(true);
        chat.appendMessage(message(QStringLiteral("image-message"),
                                   QStringLiteral("<share id=\"picture\" account=\"account-a\"/> after")));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QTRY_VERIFY(body->viewport()->isVisible());

        QTextCursor imageCursor(body->document());
        bool found = false;
        for (auto block = body->document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                if (!it.fragment().charFormat().isImageFormat())
                    continue;
                imageCursor.setPosition(it.fragment().position());
                found = true;
                break;
            }
            if (found)
                break;
        }
        QVERIFY(found);
        const QPoint imagePoint = body->cursorRect(imageCursor).topLeft() + QPoint(10, 10);
        QVERIFY(body->viewport()->rect().contains(imagePoint));

        QContextMenuEvent event(QContextMenuEvent::Mouse, imagePoint,
                                body->viewport()->mapToGlobal(imagePoint));
        QApplication::sendEvent(body->viewport(), &event);
        auto *menu = chat.findChild<QMenu *>(QStringLiteral("mamMessageContextMenu"));
        QVERIFY(menu);
        auto *copyImage = menu->findChild<QAction *>(QStringLiteral("mamCopyImage"));
        QVERIFY(copyImage); // Image pixels can be copied even without an HTTP URL.
        copyImage->trigger();
        const QImage copied = QApplication::clipboard()->image();
        QCOMPARE(copied.size(), QSize(64, 48));
        QCOMPARE(copied.pixelColor(0, 0), QColor(Qt::green));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("mamViewImage")));
        auto *copy = menu->findChild<QAction *>(QStringLiteral("mamCopyImageLink"));
        if (!expected.isEmpty()) {
            QVERIFY(copy);
            QApplication::clipboard()->setText(QStringLiteral("previous"));
            copy->trigger();
            QCOMPARE(QApplication::clipboard()->text(), expected);
        } else {
            QVERIFY(!copy);
        }
        delete menu;

        // The image action must not appear for the surrounding text.
        const QTextCursor textCursor = body->document()->find(QStringLiteral("after"));
        QVERIFY(!textCursor.isNull());
        const QPoint textPoint = body->cursorRect(textCursor).center();
        QContextMenuEvent textEvent(QContextMenuEvent::Mouse, textPoint,
                                    body->viewport()->mapToGlobal(textPoint));
        QApplication::sendEvent(body->viewport(), &textEvent);
        menu = chat.findChild<QMenu *>(QStringLiteral("mamMessageContextMenu"));
        QVERIFY(menu);
        QVERIFY(!menu->findChild<QAction *>(QStringLiteral("mamCopyImageLink")));
        QVERIFY(!menu->findChild<QAction *>(QStringLiteral("mamCopyImage")));
        QVERIFY(!menu->findChild<QAction *>(QStringLiteral("mamViewImage")));
        delete menu;
    }

    void imageActionsUseCachedOriginalAndViewer()
    {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        const QString path = cache.filePath(QStringLiteral("original.png"));
        QImage original(320, 180, QImage::Format_RGB32);
        original.fill(Qt::red);
        QVERIFY(original.save(path));

        FileSharingDeviceOpener opener;
        FileSharingItem item(QStringLiteral("image/png"));
        item.cachedPath_ = path;
        item.preview_ = QImage(60, 40, QImage::Format_RGB32);
        item.preview_.fill(Qt::blue);
        opener.items.insert(QStringLiteral("photo"), &item);

        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setMediaOpener(&opener);
        chat.setHistoryLoading(true);
        chat.appendMessage(message(QStringLiteral("photo-message"),
                                   QStringLiteral("<share id=\"photo\"/> text")));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QTRY_VERIFY(body->viewport()->isVisible());

        QTextCursor imageCursor(body->document());
        bool found = false;
        for (auto block = body->document()->begin(); block.isValid(); block = block.next())
            for (auto it = block.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().isImageFormat()) {
                    imageCursor.setPosition(it.fragment().position());
                    found = true;
                    break;
                }
        QVERIFY(found);
        const QPoint click = body->cursorRect(imageCursor).topLeft() + QPoint(10, 10);
        QContextMenuEvent context(QContextMenuEvent::Mouse, click, body->viewport()->mapToGlobal(click));
        QApplication::sendEvent(body->viewport(), &context);
        auto *menu = chat.findChild<QMenu *>(QStringLiteral("mamMessageContextMenu"));
        QVERIFY(menu);
        auto *copyImage = menu->findChild<QAction *>(QStringLiteral("mamCopyImage"));
        QVERIFY(copyImage);
        copyImage->trigger();
        QCOMPARE(QApplication::clipboard()->image().size(), original.size());
        QCOMPARE(QApplication::clipboard()->image().pixelColor(0, 0), QColor(Qt::red));

        const auto viewerWindow = []() -> MamImageViewer * {
            for (auto *widget : QApplication::topLevelWidgets())
                if (widget->isVisible() && widget->objectName() == QLatin1String("mamImageViewer"))
                    return qobject_cast<MamImageViewer *>(widget);
            return nullptr;
        };
        auto *viewAction = menu->findChild<QAction *>(QStringLiteral("mamViewImage"));
        QVERIFY(viewAction);
        QPointer<QMenu> menuGuard(menu); // Focus changes may auto-close/delete this popup.
        viewAction->trigger();
        QTRY_VERIFY(viewerWindow());
        auto *viewer = viewerWindow();
        QVERIFY(viewer->displayedImageRect().width() >= 300);
        viewer->close();
        QCoreApplication::processEvents();
        if (menuGuard)
            menuGuard->close();

        // A normal left-click also opens the viewer, but a drag does not.
        QTest::mouseClick(body->viewport(), Qt::LeftButton, Qt::NoModifier, click);
        QTRY_VERIFY(viewerWindow());
        viewerWindow()->close();
        QCoreApplication::processEvents();

        const QPoint other = click + QPoint(QApplication::startDragDistance() + 40, 0);
        QTest::mousePress(body->viewport(), Qt::LeftButton, Qt::NoModifier, click);
        QTest::mouseMove(body->viewport(), other);
        QTest::mouseRelease(body->viewport(), Qt::LeftButton, Qt::NoModifier, other);
        QVERIFY(!viewerWindow());
    }

    void mucContactMenuAndQuoteUseRoomIdentity()
    {
        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setGroupChat(true);
        chat.setHistoryLoading(true);
        auto original = message("client-id", "MUC message");
        original.setUserId("room@example.org/Peer");
        chat.appendMessage(original);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QSignalSpy contactMenu(&chat, &MamChatWidget::contactMenuRequested);
        for (const auto &name : { "mamAuthor", "mamAvatar" }) {
            auto *label = chat.itemWidget(0)->findChild<QLabel *>(name);
            QVERIFY(label);
            QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(2, 2), label->mapToGlobal(QPoint(2, 2)));
            QApplication::sendEvent(label, &event);
        }
        QCOMPARE(contactMenu.size(), 2);
        QCOMPARE(contactMenu.first().first().toString(), QString("Peer"));
        const auto openMenu = [&]() -> QMenu * {
            auto *row = chat.itemWidget(0);
            QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(2, 2), row->mapToGlobal(QPoint(2, 2)));
            QApplication::sendEvent(row, &event);
            return chat.findChild<QMenu *>("mamMessageContextMenu");
        };
        // Without a room-assigned id (old servers without stanza-ids) the
        // quote is still offered, but carries no reply reference.
        QSignalSpy quote(&chat, &MamChatWidget::replyRequested);
        auto *menu = openMenu();
        QVERIFY(menu);
        QVERIFY(menu->findChild<QAction *>("mamQuoteMessage")->isEnabled());
        menu->findChild<QAction *>("mamQuoteMessage")->trigger();
        QCOMPARE(quote.size(), 1);
        const auto plain = qvariant_cast<ChatReply>(quote.last().first());
        QVERIFY(plain.isValid());
        QVERIFY(!plain.hasReference());
        QCOMPARE(plain.author, QString("Peer"));
        QCOMPARE(plain.text, QString("MUC message"));
        delete menu;
        original.setReactionTargetId("room-assigned-id");
        original.setArchiveId("room-assigned-id");
        chat.appendMessage(original);
        menu = openMenu();
        QVERIFY(menu);
        QVERIFY(menu->findChild<QAction *>("mamQuoteMessage")->isEnabled());
        menu->findChild<QAction *>("mamQuoteMessage")->trigger();
        QCOMPARE(quote.size(), 2);
        QCOMPARE(qvariant_cast<ChatReply>(quote.last().first()).id, QString("room-assigned-id"));
        delete menu;
    }

    void replyPreviewResolvesLateArchiveTargetAndStripsOnlyMarkedFallback()
    {
        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setHistoryLoading(true);
        auto reply = message("reply", "> old fallback\nAnswer", 5);
        reply.setReply({"origin", "peer@example.org", "Peer", "old fallback"}, "Answer", true);
        chat.appendMessage(reply);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *row = chat.itemWidget(0);
        QCOMPARE(row->findChild<ChatMessageText *>()->toPlainText(), QString("Answer"));
        auto *preview = row->findChild<MessagePreview *>("mamReplyPreview");
        QVERIFY(preview);
        QCOMPARE(preview->findChild<QTextEdit *>("previewText")->toPlainText(), QString("old fallback"));
        auto original = message("client", "The original message");
        original.setUserId("peer@example.org/phone");
        original.setReactionTargetId("origin");
        chat.prependArchivePage({qMakePair(QString("archive-original"), original)});
        QTRY_COMPARE(preview->findChild<QTextEdit *>("previewText")->toPlainText(), QString("The original message"));
        QCOMPARE(chat.messageCount(), 2);
        auto *excerpt = preview->findChild<QTextEdit *>("previewText");
        excerpt->selectAll();
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(2, 2), excerpt->viewport()->mapToGlobal(QPoint(2, 2)));
        QApplication::sendEvent(excerpt->viewport(), &event);
        auto *menu = chat.findChild<QMenu *>("mamMessageContextMenu");
        QVERIFY(menu);
        menu->findChild<QAction *>("mamCopyMessage")->trigger();
        QCOMPARE(QApplication::clipboard()->text(), QString("The original message"));
        QVERIFY(menu->findChild<QAction *>("mamQuoteMessage"));
        delete menu;
        auto corrected = original;
        corrected.setMessageId("correction");
        corrected.setReplaceId("client");
        corrected.setPlainText("Updated original");
        corrected.setRevisionTime(original.dateTime().addSecs(20));
        chat.appendMessage(corrected);
        QTRY_COMPARE(excerpt->toPlainText(), QString("Updated original"));
    }

    void replyPreviewShowsTheQuotedMessageWithoutNestedQuotes()
    {
        FileSharingDeviceOpener opener;
        FileSharingDeviceOpener account;
        opener.accounts.insert(QStringLiteral("account-a"), &account);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(600, 400, QImage::Format_RGB32);
        image.preview_.fill(QColor(0, 170, 0));
        account.items.insert(QStringLiteral("picture"), &image);

        MamChatWidget chat;
        chat.resize(560, 700);
        chat.setHistoryLoading(true);
        chat.setMediaOpener(&opener);
        chat.setQuoteColor(QColor("#707070"));
        QPalette colors = chat.palette();
        colors.setColor(QPalette::Link, QColor("#2a6fb0"));
        chat.setPalette(colors);
        auto original = message(QStringLiteral("original"),
                                QStringLiteral("&gt; Someone:<br>&gt; an older quote<br>Look at "
                                               "<a href=\"https://psi-im.org\">psi-im.org</a><br>"
                                               "&gt; an interleaved quote<br>"
                                               "<share id=\"picture\" account=\"account-a\"/><br>The end"));
        original.setReactionTargetId(QStringLiteral("original-id"));
        chat.appendMessage(original);
        auto reply = message(QStringLiteral("reply"), QStringLiteral("Answer"), 1, true);
        reply.setReply({ QStringLiteral("original-id"), {}, QStringLiteral("Peer"), QStringLiteral("fallback") });
        chat.appendMessage(reply);
        // The quoted message is not here: the quoting client's fallback text.
        auto orphan = message(QStringLiteral("orphan"), QStringLiteral("Another answer"), 2, true);
        orphan.setReply({ QStringLiteral("missing-id"), {}, QStringLiteral("Peer"),
                          QStringLiteral("> a deeper quote\nVisit https://example.org now\n> skipped\ntail") });
        chat.appendMessage(orphan);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(2));

        auto *preview = chat.itemWidget(1)->findChild<MessagePreview *>(QStringLiteral("mamReplyPreview"));
        QVERIFY(preview && preview->property("replyResolved").toBool());
        auto *text = preview->findChild<ChatMessageText *>(QStringLiteral("previewText"));
        QVERIFY(text);
        // The row's own text is still the message, not its quote.
        QCOMPARE(chat.itemWidget(1)->findChild<ChatMessageText *>(QStringLiteral("mamBody"))->toPlainText(),
                 QStringLiteral("Answer"));
        const QString shown = text->toPlainText();
        QVERIFY2(shown.startsWith(QStringLiteral("Look at psi-im.org")), qPrintable(shown));
        QVERIFY2(shown.endsWith(QStringLiteral("The end")), qPrintable(shown));
        for (const char *hidden : { "Someone", "an older quote", "an interleaved quote", ">" })
            QVERIFY2(!shown.contains(QLatin1String(hidden)), qPrintable(shown));
        // Rich content as in the message: the link and the picture.
        const QTextCursor link = text->document()->find(QStringLiteral("psi-im.org"));
        QCOMPARE(link.charFormat().anchorHref(), QStringLiteral("https://psi-im.org"));
        QCOMPARE(link.charFormat().foreground().color(), QColor("#2a6fb0"));
        bool picture = false;
        for (auto block = text->document()->begin(); block.isValid(); block = block.next())
            for (auto it = block.begin(); !it.atEnd(); ++it)
                picture = picture || it.fragment().charFormat().isImageFormat();
        QVERIFY(picture);
        // Everything else in the quote's colour.
        QCOMPARE(text->palette().color(QPalette::Text), QColor("#707070"));

        // Collapsed, the picture is a thumbnail as high as three lines;
        // expanded, it is as large as in the message.
        auto *more = preview->findChild<LinkButton *>(QStringLiteral("previewShowMore"));
        QTRY_VERIFY(more->isVisible());
        const int collapsed = text->height();
        QVERIFY(collapsed <= 3 * text->fontMetrics().lineSpacing() + 1);
        QTest::mouseClick(more, Qt::LeftButton);
        QTRY_VERIFY(text->height() > collapsed + 100);
        QTRY_VERIFY(preview->rect().contains(QRect(text->mapTo(preview, QPoint()), text->size())));

        auto *fallback = chat.itemWidget(2)->findChild<MessagePreview *>(QStringLiteral("mamReplyPreview"));
        QVERIFY(fallback && !fallback->property("replyResolved").toBool());
        auto *fallbackText = fallback->findChild<ChatMessageText *>(QStringLiteral("previewText"));
        QCOMPARE(fallbackText->toPlainText(), QStringLiteral("Visit https://example.org now\ntail"));
        QCOMPARE(fallbackText->document()->find(QStringLiteral("https://example.org")).charFormat().anchorHref(),
                 QStringLiteral("https://example.org"));
        // Two short lines fit: nothing more to show.
        QTRY_VERIFY(!fallback->findChild<LinkButton *>(QStringLiteral("previewShowMore"))->isVisible());
        QCOMPARE(fallbackText->height(), fallbackText->heightForWidth(fallbackText->width()));
    }

    void quotesWithinAMessageAreShownAsQuotes()
    {
        MamChatWidget chat;
        chat.resize(560, 400);
        chat.setHistoryLoading(true);
        chat.setQuoteColor(QColor("#707070"));
        QPalette colors = chat.palette();
        colors.setColor(QPalette::Text, QColor("#101010"));
        colors.setColor(QPalette::Link, QColor("#2a6fb0"));
        chat.setPalette(colors);
        chat.appendMessage(message(QStringLiteral("quotes"),
                                   QStringLiteral("Top<br>&gt; first with <a href=\"https://psi-im.org\">a link</a><br>"
                                                  "&gt; &gt; second<br>&gt;&gt; also second<br>Answer")));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QVERIFY(body);
        // The markers are gone; each quoted line is a block of its own.
        QCOMPARE(body->toPlainText(), QStringLiteral("Top\nfirst with a link\nsecond\nalso second\nAnswer"));
        const auto blockOf = [body](const QString &text) { return body->document()->find(text).block(); };
        QCOMPARE(blockOf(QStringLiteral("Top")).blockFormat().leftMargin(), 0.0);
        QCOMPARE(blockOf(QStringLiteral("first")).blockFormat().leftMargin(), 10.0);
        QCOMPARE(blockOf(QStringLiteral("second")).blockFormat().leftMargin(), 20.0);
        QCOMPARE(blockOf(QStringLiteral("also second")).blockFormat().leftMargin(), 20.0);
        QCOMPARE(blockOf(QStringLiteral("Answer")).blockFormat().leftMargin(), 0.0);
        // Quoted text in the quote colour, links in theirs, the rest as usual.
        QCOMPARE(body->document()->find(QStringLiteral("first")).charFormat().foreground().color(), QColor("#707070"));
        QCOMPARE(body->document()->find(QStringLiteral("a link")).charFormat().foreground().color(), QColor("#2a6fb0"));
        QVERIFY(body->document()->find(QStringLiteral("Answer")).charFormat().foreground().color() != QColor("#707070"));
        // A bar per level beside the quoted lines.
        QTRY_VERIFY(body->height() >= body->heightForWidth(body->width()));
        const QImage shot  = body->viewport()->grab().toImage();
        const auto   barAt = [&](const QString &text, int x) {
            const QRect line = body->cursorRect(body->document()->find(text));
            return shot.pixelColor(x, line.center().y()) == QColor("#707070");
        };
        QVERIFY(barAt(QStringLiteral("first"), 1));
        QVERIFY(!barAt(QStringLiteral("first"), 11));
        QVERIFY(barAt(QStringLiteral("second"), 1) && barAt(QStringLiteral("second"), 11));
        QVERIFY(!barAt(QStringLiteral("Top"), 1));
        // Copying gives the markers back.
        body->selectAll();
        body->copy();
        QCOMPARE(QApplication::clipboard()->text(),
                 QStringLiteral("Top\n> first with a link\n>> second\n>> also second\nAnswer"));
        // The quote colour follows the option.
        chat.setQuoteColor(QColor("#a0a4a8"));
        QCOMPARE(body->document()->find(QStringLiteral("first")).charFormat().foreground().color(), QColor("#a0a4a8"));
    }

    void inlineReplyPreviewContributesToRowHeight_data()
    {
        QTest::addColumn<bool>("classic");
        QTest::addColumn<bool>("longQuote");
        QTest::newRow("modern") << false << false;
        QTest::newRow("classic") << true << false;
        QTest::newRow("modern-long") << false << true;
        QTest::newRow("classic-long") << true << true;
    }

    void inlineReplyPreviewContributesToRowHeight()
    {
        QFETCH(bool, classic);
        QFETCH(bool, longQuote);
        MamChatWidget chat;
        chat.resize(550, 450);
        chat.setHistoryLoading(true);
        chat.setQuoteColor(QColor("#707070"));
        if (classic)
            chat.setTheme({ "classic", "Classic", NativeChatTheme::Classic, 0, 4, {} });
        auto reply = message("reply", "1234");
        QString quoted = "Quoted first line\nQuoted second line";
        if (longQuote)
            for (int i = 0; i < 50; ++i)
                quoted += QString("\nAdditional quoted line %1").arg(i);
        reply.setReply({ "original", "peer@example.org", "Peer", quoted });
        chat.appendMessage(reply);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *row = chat.itemWidget(0);
        auto *preview = row->findChild<MessagePreview *>("mamReplyPreview");
        QVERIFY(preview);
        auto *excerpt = preview->findChild<QTextEdit *>("previewText");
        auto *title = preview->findChild<QLabel *>("previewTitle");
        auto *body = row->findChild<ChatMessageText *>();
        QVERIFY(excerpt && title && body);
        QTRY_VERIFY2(preview->height() >= preview->minimumSizeHint().height(),
                     qPrintable(QString("preview height=%1 minimum=%2 row=%3 rowHint=%4")
                                    .arg(preview->height()).arg(preview->minimumSizeHint().height())
                                    .arg(row->height()).arg(row->sizeHint().height())));
        const auto insideRow = [row](QWidget *child) {
            return row->rect().contains(QRect(child->mapTo(row, QPoint()), child->size()));
        };
        QTRY_VERIFY(insideRow(preview));
        QTRY_VERIFY2(preview->rect().contains(QRect(excerpt->mapTo(preview, QPoint()), excerpt->size())),
                     qPrintable(QString("preview=%1x%2 hint=%3x%4 excerpt=%5,%6 %7x%8 row=%9")
                                    .arg(preview->width()).arg(preview->height())
                                    .arg(preview->sizeHint().width()).arg(preview->sizeHint().height())
                                    .arg(excerpt->mapTo(preview, QPoint()).x()).arg(excerpt->mapTo(preview, QPoint()).y())
                                    .arg(excerpt->width()).arg(excerpt->height()).arg(row->height())));
        QTRY_VERIFY(excerpt->height() >= 2 * excerpt->fontMetrics().lineSpacing());
        QVERIFY(title->height() >= title->fontMetrics().height());
        QCOMPARE(excerpt->palette().color(QPalette::Text), QColor("#707070"));
        QCOMPARE(title->palette().color(QPalette::WindowText), QColor("#707070"));
        QVERIFY(body->mapTo(row, QPoint()).y() >= preview->mapTo(row, QPoint()).y() + preview->height());
        QVERIFY(insideRow(body));

        if (longQuote) {
            auto *more = preview->findChild<LinkButton *>("previewShowMore");
            QVERIFY(more->isVisible());
            const int collapsedRowHeight = row->height();
            QTest::mouseClick(more, Qt::LeftButton);
            QTRY_VERIFY(row->height() > collapsedRowHeight);
            QPalette links = chat.palette();
            links.setColor(QPalette::Link, QColor("#34789c"));
            chat.setPalette(links);
            QTRY_COMPARE(more->linkColor(), QColor("#34789c"));
            chat.setQuoteColor(QColor("#a0a4a8"));
            QVERIFY(preview->isExpanded());
            QCOMPARE(excerpt->palette().color(QPalette::Text), QColor("#a0a4a8"));
            QTRY_VERIFY(insideRow(preview) && insideRow(body));
            QVERIFY(preview->rect().contains(QRect(more->mapTo(preview, QPoint()), more->size())));
            QTest::mouseClick(more, Qt::LeftButton);
            QTRY_COMPARE(row->height(), collapsedRowHeight);
        }
        for (int width : { 280, 700, 550 }) {
            chat.resize(width, chat.height());
            QTRY_VERIFY(insideRow(preview) && insideRow(body));
            QTRY_VERIFY(preview->rect().contains(QRect(excerpt->mapTo(preview, QPoint()), excerpt->size())));
        }
    }

    void previewColorsCanOverrideOrInheritText_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("overrideText");
        QTest::addColumn<bool>("stylesheet");
        for (bool stylesheet : { false, true }) {
            const QByteArray suffix = stylesheet ? "-stylesheet" : "";
            QTest::newRow(("light-quote" + suffix).constData()) << false << true << stylesheet;
            QTest::newRow(("dark-quote" + suffix).constData()) << true << true << stylesheet;
            QTest::newRow(("light-link" + suffix).constData()) << false << false << stylesheet;
            QTest::newRow(("dark-link" + suffix).constData()) << true << false << stylesheet;
        }
    }

    void previewColorsCanOverrideOrInheritText()
    {
        QFETCH(bool, dark);
        QFETCH(bool, overrideText);
        QFETCH(bool, stylesheet);
        QWidget window;
        window.setObjectName(QStringLiteral("previewPaletteHost"));
        if (stylesheet)
            window.setStyleSheet(QStringLiteral("QWidget#previewPaletteHost { padding: 0px; }"));
        window.resize(400, 400);
        QPalette parentPalette = window.palette();
        parentPalette.setColor(QPalette::Text, dark ? QColor("#eeeeee") : QColor("#202020"));
        parentPalette.setColor(QPalette::WindowText, dark ? QColor("#dddddd") : QColor("#303030"));
        window.setPalette(parentPalette);
        const QColor quoteColor(dark ? "#a0a4a8" : "#707070");
        const QColor lineColor = overrideText ? quoteColor : QColor("#4080c0");
        auto *layout = new QVBoxLayout(&window);
        auto *preview = new MessagePreview(lineColor, overrideText ? quoteColor : QColor(), &window);
        // Qt's stylesheet propagation policy can use the application palette
        // instead of a parent's custom palette. With no override we must match
        // ordinary Qt controls, including that policy, rather than freeze a
        // manually resolved parent color into the preview.
        QTextBrowser normalText(preview);
        QLabel normalTitle(preview);
        normalText.hide();
        normalTitle.hide();
        normalText.ensurePolished();
        normalTitle.ensurePolished();
        layout->addWidget(preview);
        layout->addStretch();
        preview->setContent("Author", "Selectable quoted excerpt");
        window.show();
        auto *bar = preview->findChild<QFrame *>("previewBar");
        auto *title = preview->findChild<QLabel *>("previewTitle");
        auto *excerpt = preview->findChild<QTextEdit *>("previewText");
        QVERIFY(bar && title && excerpt);
        QTRY_VERIFY(bar->height() >= excerpt->height());
        QCOMPARE(excerpt->palette().color(QPalette::Text),
                 overrideText ? quoteColor : normalText.palette().color(QPalette::Text));
        QCOMPARE(excerpt->viewport()->palette().color(QPalette::Text), excerpt->palette().color(QPalette::Text));
        QCOMPARE(title->palette().color(QPalette::WindowText),
                 overrideText ? quoteColor : normalTitle.palette().color(QPalette::WindowText));
        const QImage line = bar->grab().toImage();
        QCOMPARE(line.pixelColor(line.width() / 2, line.height() / 2), lineColor);

        // A parent palette change follows ordinary Qt color resolution, while
        // explicitly supplied quote colors remain independent of those roles.
        parentPalette.setColor(QPalette::Text, QColor("#467856"));
        parentPalette.setColor(QPalette::WindowText, QColor("#566876"));
        window.setPalette(parentPalette);
        QTRY_COMPARE(excerpt->palette().color(QPalette::Text),
                     overrideText ? quoteColor : normalText.palette().color(QPalette::Text));
        QTRY_COMPARE(title->palette().color(QPalette::WindowText),
                     overrideText ? quoteColor : normalTitle.palette().color(QPalette::WindowText));
        QTRY_COMPARE(excerpt->viewport()->palette().color(QPalette::Text), excerpt->palette().color(QPalette::Text));
        preview->setColors(lineColor); // Clear a previous text override, too.
        QCOMPARE(excerpt->palette().color(QPalette::Text), normalText.palette().color(QPalette::Text));
        QCOMPARE(title->palette().color(QPalette::WindowText), normalTitle.palette().color(QPalette::WindowText));
        QCOMPARE(excerpt->palette().color(QPalette::Highlight), normalText.palette().color(QPalette::Highlight));
    }

    void linkButtonPaintsOnlyTextAndKeepsButtonInput()
    {
        LinkButton link;
        link.setAttribute(Qt::WA_TranslucentBackground);
        QFont font = link.font();
        font.setPixelSize(24);
        font.setBold(true);
        link.setFont(font);
        link.setText("More");
        link.setLinkColor(QColor("#3070c0"));
        const QSize textSize = link.fontMetrics().size(Qt::TextShowMnemonic, link.text());
        QCOMPARE(link.sizeHint(), textSize); // No native button padding.
        link.resize(link.sizeHint());
        link.show();
        const auto capture = [&] {
            QImage result(link.size(), QImage::Format_ARGB32_Premultiplied);
            result.fill(Qt::transparent);
            link.render(&result, QPoint(), QRegion(), QWidget::DrawChildren);
            return result;
        };
        const auto maxAlpha = [](const QImage &image) {
            int alpha = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    alpha = qMax(alpha, image.pixelColor(x, y).alpha());
            return alpha;
        };
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&link, &leave);
        const auto resting = capture();
        QVERIFY(qAbs(maxAlpha(resting) - 204) <= 1);
        QCOMPARE(resting.pixelColor(resting.width() - 1, 0).alpha(), 0); // No background or frame.
        enterWidget(&link);
        const auto hovered = capture();
        QCOMPARE(maxAlpha(hovered), 255);
        QCOMPARE(hovered.pixelColor(hovered.width() - 1, 0).alpha(), 0);
        const auto strongColor = [](const QImage &image) {
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (image.pixelColor(x, y).alpha() == 255)
                        return image.pixelColor(x, y);
            return QColor();
        };
        QCOMPARE(strongColor(hovered), QColor("#3070c0"));
        link.setLinkColor(QColor("#906030"));
        QCOMPARE(strongColor(capture()), QColor("#906030"));
        QApplication::sendEvent(&link, &leave);
        QVERIFY(qAbs(maxAlpha(capture()) - 204) <= 1);

        QSignalSpy clicked(&link, &LinkButton::clicked);
        QTest::mouseClick(&link, Qt::RightButton);
        QCOMPARE(clicked.size(), 0);
        QTest::mouseClick(&link, Qt::LeftButton);
        QCOMPARE(clicked.size(), 1);
        QTest::mousePress(&link, Qt::LeftButton);
        QTest::mouseRelease(&link, Qt::LeftButton, Qt::NoModifier, QPoint(-5, -5));
        QCOMPARE(clicked.size(), 1); // Releasing outside cancels a press.
        QTest::keyClick(&link, Qt::Key_Space);
        QTest::keyClick(&link, Qt::Key_Return);
        QCOMPARE(clicked.size(), 3);
        link.setEnabled(false);
        QTest::mouseClick(&link, Qt::LeftButton);
        QTest::keyClick(&link, Qt::Key_Return);
        QCOMPARE(clicked.size(), 3);
    }

    void previewExpandsWithoutDiscardingSelectableContent()
    {
        QWidget window;
        window.resize(400, 400);
        auto *layout = new QVBoxLayout(&window);
        auto *preview = new MessagePreview(Qt::gray, {}, &window);
        layout->addWidget(preview);
        layout->addStretch();
        QString text;
        for (int i = 0; i < 100; ++i)
            text += QString("Selectable line %1\n").arg(i);
        preview->setContent("Author", text);
        preview->setClosable(true);
        window.show();
        auto *browser = preview->findChild<QTextEdit *>("previewText");
        auto *more = preview->findChild<LinkButton *>("previewShowMore");
        QTRY_VERIFY(more->isVisible());
        const int collapsedHeight = browser->height();
        browser->selectAll();
        browser->copy();
        QCOMPARE(QApplication::clipboard()->text(), text);
        QTest::mouseClick(more, Qt::LeftButton);
        QVERIFY(preview->isExpanded());
        QTRY_VERIFY(browser->height() > collapsedHeight);
        QTRY_VERIFY(browser->verticalScrollBar()->maximum() > 0);
        QVERIFY(browser->height() <= window.height() / 2);
        QCOMPARE(browser->toPlainText(), text);
        QTest::mouseClick(more, Qt::LeftButton);
        QVERIFY(!preview->isExpanded());
        QCOMPARE(browser->height(), collapsedHeight);
        QCOMPARE(browser->verticalScrollBar()->value(), 0);
        QSignalSpy dismissed(preview, &MessagePreview::dismissed);
        QTest::mouseClick(preview->findChild<QPushButton *>("previewDismiss"), Qt::LeftButton);
        QCOMPARE(dismissed.size(), 1);
    }

    void hoveredRowSurfaceFollowsPointerAndPreservesSelection_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("classic");
        QTest::newRow("light-modern") << false << false;
        QTest::newRow("dark-modern") << true << false;
        QTest::newRow("light-classic") << false << true;
        QTest::newRow("dark-classic") << true << true;
    }

    void hoveredRowSurfaceFollowsPointerAndPreservesSelection()
    {
        QFETCH(bool, dark);
        QFETCH(bool, classic);
        MamChatWidget chat;
        chat.resize(550, 350);
        chat.setHistoryLoading(true);
        if (classic)
            chat.setTheme({ "classic", "Classic", NativeChatTheme::Classic, 0, 4, {} });
        QPalette palette = chat.palette();
        const QColor base = dark ? QColor(25, 25, 25) : QColor(Qt::white);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Window, base);
        palette.setColor(QPalette::Text, dark ? Qt::white : Qt::black);
        palette.setColor(QPalette::Highlight, QColor(0, 120, 230));
        chat.setPalette(palette);
        chat.appendMessage(message("hover-first", "First <a href='https://example.org'>message</a>"));
        chat.appendMessage(message("hover-second", "Second message", 1));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(chat.viewport(), &leave);
        auto *first = chat.itemWidget(0);
        auto *second = chat.itemWidget(1);
        auto *body = first->findChild<ChatMessageText *>();
        QVERIFY(body);
        body->selectAll();
        const auto selection = body->textCursor().selectedText();
        const int revision = body->document()->revision();
        enterWidget(first);
        const auto hover = chat.hoverHighlightSurfaceColor();
        QVERIFY(hover != base);
        QCOMPARE(first->palette().color(QPalette::Base), hover);
        QCOMPARE(second->palette().color(QPalette::Base), base);
        QCOMPARE(body->textCursor().selectedText(), selection);
        QCOMPARE(body->document()->revision(), revision);
        // Check actual pixels, including QTextEdit's child viewport: changing
        // just the outer list's paint does not show through opaque row surfaces.
        const auto point = body->viewport()->mapTo(&chat, QPoint(body->viewport()->width() - 3,
                                                                body->viewport()->height() - 3));
        const auto image = chat.grab().toImage();
        QCOMPARE(image.pixelColor(qRound(point.x() * image.devicePixelRatio()),
                                  qRound(point.y() * image.devicePixelRatio())).rgba(), hover.rgba());
        auto *actions = chat.findChild<QFrame *>("mamMessageActions");
        QVERIFY(actions);
        QTRY_VERIFY(actions->isVisible());
        QApplication::sendEvent(first, &leave);
        enterWidget(actions);
        QCOMPARE(first->palette().color(QPalette::Base), hover);
        enterWidget(second);
        QCOMPARE(first->palette().color(QPalette::Base), base);
        QCOMPARE(second->palette().color(QPalette::Base), hover);
        chat.selectMessageRange(1, 1);
        const auto selected = second->palette().color(QPalette::Base);
        QVERIFY(selected != hover);
        QApplication::sendEvent(chat.viewport(), &leave);
        QCOMPARE(second->palette().color(QPalette::Base), selected);
        chat.cancelMessageSelection();
        QCOMPARE(second->palette().color(QPalette::Base), base);
        enterWidget(first);
        palette.setColor(QPalette::Highlight, QColor(200, 80, 20));
        chat.setPalette(palette);
        QTRY_COMPARE(first->palette().color(QPalette::Base), chat.hoverHighlightSurfaceColor());
        QApplication::setActiveWindow(nullptr);
        QTRY_COMPARE(first->palette().color(QPalette::Base), base);
        enterWidget(first);
        chat.hide();
        QCOMPARE(first->palette().color(QPalette::Base), base);
    }

    void emoteUsesSenderPrefixWithoutChangingStoredMessage()
    {
        MamChatWidget chat;
        chat.resize(500, 280);
        chat.setHistoryLoading(true);
        auto action = message(QStringLiteral("action"), QStringLiteral("танцует"));
        action.setEmote();
        action.setNick(QStringLiteral("Алиса <&>"));
        chat.appendMessage(action);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QCOMPARE(body->toPlainText(), QStringLiteral("*Алиса <&> танцует"));
        QCOMPARE(chat.messageCount(), 1);
        // Regular messages are not decorated with the emote prefix.
        chat.appendMessage(message(QStringLiteral("normal"), QStringLiteral("танцует"), 1));
        QTRY_VERIFY(chat.itemWidget(1));
        QCOMPARE(chat.itemWidget(1)->findChild<ChatMessageText *>()->toPlainText(), QStringLiteral("танцует"));
    }

    void distinctPayloadsWithReusedClientIdAreNotDiscarded()
    {
        MamChatWidget chat;
        chat.setHistoryLoading(true);
        chat.appendMessage(message("same-id", "first"));
        chat.appendMessage(message("same-id", "second", 1));
        QCOMPARE(chat.messageCount(), 2);
    }

    void resolvedCorrectionUpdatesKnownArchiveKeyWithoutMovingOriginalTime()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        auto original = message("a", "original");
        original.setHistoryId(1);
        chat.prependArchivePage({ qMakePair(QString("uid-a"), original) });
        auto corrected = original;
        corrected.setMessageId("b");
        corrected.setReplaceId("a");
        corrected.setPlainText("corrected");
        corrected.setRevisionTime(original.dateTime().addSecs(30));
        chat.prependArchivePage({ qMakePair(QString("uid-a"), corrected) });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.messageCount(), 1);
        QCOMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("corrected"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp")->toolTip(),
                 MamChatWidget::fullTimestamp(original.dateTime()));
    }

    void reactionsReplaceAnActorsWholeSetAndSurviveLateMessageArrival()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        MessageView reaction(MessageView::Reactions);
        reaction.setReactionsId("original");
        reaction.setReactions({ QString::fromUtf8("👍"), QString::fromUtf8("❤️") });
        chat.appendMessage(reaction);
        chat.appendMessage(message("original", "hello"));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.messageCount(), 1);
        QCOMPARE(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").size(), 2);
        reaction.setReactions({ QString::fromUtf8("❤️") });
        chat.appendMessage(reaction);
        QTRY_COMPARE(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").size(), 1);
        QSignalSpy outgoing(&chat, &MamChatWidget::outgoingReactions);
        chat.toggleReaction("original", QString::fromUtf8("❤️"));
        QCOMPARE(outgoing.size(), 1);
        QCOMPARE(qvariant_cast<QSet<QString>>(outgoing.first().at(1)).size(), 1);
        chat.toggleReaction("original", QString::fromUtf8("❤️"));
        QCOMPARE(qvariant_cast<QSet<QString>>(outgoing.last().at(1)).size(), 0);
        reaction.setReactions({});
        chat.appendMessage(reaction);
        QTRY_VERIFY(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").isEmpty());
    }

    void floatingReactionBarUsesPsiChooserSignalAndOriginalTarget()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        auto original = message("client-id", "hello");
        original.setReactionTargetId("origin-id");
        chat.appendMessage(original);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        QSignalSpy chooser(&chat, &MamChatWidget::emojiChooserRequested);
        auto      *choose = bar->findChild<QPushButton *>("mamChooseReaction");
        QTest::mouseClick(choose, Qt::LeftButton);
        QCOMPARE(chooser.size(), 1);
        QCOMPARE(chooser.first().first().toString(), QString("origin-id"));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(bar, &leave);
        QTRY_VERIFY(!bar->isVisible());
    }

    void messageActionsRemainVisibleAcrossRowChildrenAndPadding()
    {
        MamChatWidget chat;
        chat.resize(600, 400);
        chat.setHistoryLoading(true);
        auto own = message("actions-client", "Message text<br>Second line<br>Third line", 0, true);
        own.setReactionTargetId("actions-origin");
        own.setRetractionTargetId("actions-client");
        chat.appendMessage(own);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *row = chat.itemWidget(0);
        auto *body = row->findChild<ChatMessageText *>();
        auto *author = row->findChild<QLabel *>("mamAuthor");
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QVERIFY(bar);
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(bar->graphicsEffect());
        QVERIFY(body && author && opacity);

        QTest::mouseMove(body->viewport(), body->viewport()->rect().center());
        QTRY_VERIFY(bar->isVisible());
        QTRY_COMPARE(opacity->opacity(), 1.0);
        // The right/bottom padding belongs to the message row but to none of
        // its text widgets. No child Enter follows the text viewport's Leave.
        const QPoint padding(row->width() - 3, row->height() - 3);
        QVERIFY(!body->viewport()->rect().contains(body->viewport()->mapFrom(row, padding)));
        QTest::mouseMove(row, padding);
        QTest::qWait(350); // Longer than hide delay plus fade duration.
        QVERIFY(bar->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);
        QTest::mouseMove(author, author->rect().center());
        QTest::mouseMove(row, padding);
        QTest::qWait(350);
        QVERIFY(bar->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);

        auto *choose = bar->findChild<QPushButton *>("mamChooseReaction");
        auto *more = bar->findChild<QPushButton *>("mamMoreActions");
        QVERIFY(choose && more && more->isVisible());
        QTest::mouseMove(choose, choose->rect().center());
        QTest::qWait(350);
        QVERIFY(bar->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);
        auto *quick = bar->findChild<QPushButton *>("mamQuickReaction");
        QVERIFY(quick && quick->isVisible());
        QTest::mouseMove(quick, quick->rect().center());
        QTest::qWait(350);
        QVERIFY(bar->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);
        QTest::mouseMove(more, more->rect().center());
        QTest::qWait(350);
        QVERIFY(bar->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);

        const QPoint outside(chat.viewport()->width() / 2, chat.viewport()->height() - 3);
        QVERIFY(!QRect(row->mapTo(chat.viewport(), QPoint()), row->size()).contains(outside));
        QTest::mouseMove(chat.viewport(), outside);
        QTRY_VERIFY(!bar->isVisible());
    }

    void messageActionsKeepRetractionBehindMoreMenu()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        auto own = message("message-id", "mine", 0, true);
        own.setReactionTargetId("origin-id");
        own.setRetractionTargetId("message-id");
        chat.appendMessage(own);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        // Left to right: quick slot, react, more. No direct retract button.
        auto *quick  = bar->findChild<QWidget *>("mamQuickReactions");
        auto *choose = bar->findChild<QPushButton *>("mamChooseReaction");
        auto *more   = bar->findChild<QPushButton *>("mamMoreActions");
        QVERIFY(quick && choose && more);
        QVERIFY(more->isVisible());
        QCOMPARE(quick->maximumWidth(), 0);
        QVERIFY(choose->mapTo(bar, QPoint()).x() < more->mapTo(bar, QPoint()).x());
        for (auto *button : bar->findChildren<QPushButton *>())
            QVERIFY(!button->text().contains("Retract"));

        // Expanding quick reactions grows leftwards; react/more stay put.
        const QPoint chooseAt = choose->mapTo(chat.viewport(), QPoint());
        QApplication::sendEvent(choose, &enter);
        QTRY_VERIFY(quick->maximumWidth() > 0);
        QTRY_COMPARE(quick->maximumWidth(), quick->layout()->sizeHint().width());
        QCOMPARE(choose->mapTo(chat.viewport(), QPoint()), chooseAt);
        QVERIFY(quick->mapTo(bar, QPoint()).x() < choose->mapTo(bar, QPoint()).x());

        // XEP-0424: a direct chat retracts message@id, not the origin-id.
        QSignalSpy retraction(&chat, &MamChatWidget::outgoingMessageRetraction);
        more->click();
        auto *retract = chat.findChild<QAction *>("mamRetractMessage");
        QVERIFY(retract);
        retract->trigger();
        QCOMPARE(retraction.size(), 1);
        QCOMPARE(retraction.first().first().toString(), QString("message-id"));
        QTRY_COMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("Message retracted"));
        if (auto *menu = chat.findChild<QMenu *>("mamMessageMenu"))
            menu->hide();
    }

    void peerMessageActionsHaveNoMoreMenu()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("peer-id", "theirs"));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        QVERIFY(bar->findChild<QPushButton *>("mamChooseReaction")->isVisible());
        QVERIFY(!bar->findChild<QPushButton *>("mamMoreActions")->isVisible());
    }

    void roomRowLearnsArchiveStanzaIdForActions()
    {
        MamChatWidget room;
        room.setGroupChat(true);
        room.resize(500, 250);
        room.setHistoryLoading(true);
        // Live copy without a room-assigned stanza-id: only the bounded
        // legacy message@id fallback, addressed through a local row key.
        room.appendMessage(message("client-id", "hello"));
        room.show();
        QTRY_VERIFY(room.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(room.itemWidget(0), &enter);
        auto *bar = room.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        QSignalSpy chooser(&room, &MamChatWidget::emojiChooserRequested);
        bar->findChild<QPushButton *>("mamChooseReaction")->click();
        QCOMPARE(chooser.size(), 1);
        QVERIFY(chooser.last().first().toString().startsWith(QStringLiteral("legacy-row:")));

        // The archived copy of the same message carries the stanza-id.
        auto archived = message("client-id", "hello");
        archived.setReactionTargetId("room-stanza-id");
        archived.setRetractionTargetId("room-stanza-id");
        archived.setSpooled(true);
        QCOMPARE(room.prependArchivePage({ qMakePair(QString("room-stanza-id"), archived) }), 0);
        QCOMPARE(room.messageCount(), 1);
        QTRY_VERIFY(room.itemWidget(0));
        QApplication::sendEvent(room.itemWidget(0), &enter);
        QTRY_VERIFY(bar->isVisible());
        bar->findChild<QPushButton *>("mamChooseReaction")->click();
        QCOMPARE(chooser.size(), 2);
        QCOMPARE(chooser.last().first().toString(), QString("room-stanza-id"));
    }

    void plainTextQuoteIsNotResolvedToAnIdLessRoomRow()
    {
        // In a room without stanza-ids every row's reaction target is empty;
        // an id-less quote must keep its own author and text, not "resolve"
        // to whichever row happens to be the only candidate.
        MamChatWidget room;
        room.setGroupChat(true);
        room.resize(500, 300);
        room.setHistoryLoading(true);
        auto other = message("m1", "unrelated message", 1);
        other.setUserId(QStringLiteral("room@example.org/Peer"));
        room.appendMessage(other);
        auto answer = message("m2", "> Vitozz:\n> quoted\nanswer", 2);
        answer.setUserId(QStringLiteral("room@example.org/Me"));
        answer.setReply({ {}, {}, "Vitozz", "quoted" }, "answer", true);
        room.appendMessage(answer);
        room.show();
        QTRY_VERIFY(room.itemWidget(1));
        auto *preview = room.itemWidget(1)->findChild<MessagePreview *>("mamReplyPreview");
        QVERIFY(preview);
        QCOMPARE(preview->property("replyResolved").toBool(), false);
        QCOMPARE(room.itemWidget(1)->findChild<ChatMessageText *>()->toPlainText(), QString("answer"));
        auto *title = preview->findChild<QLabel *>("previewTitle");
        QVERIFY(title);
        QCOMPARE(title->text(), QString("Vitozz"));
        QVERIFY(!title->isHidden());

        // A quote without an author shows no placeholder heading at all.
        auto anonymous = message("m3", "> quoted\nanswer", 3);
        anonymous.setUserId(QStringLiteral("room@example.org/Me"));
        anonymous.setReply({ {}, {}, {}, "quoted" }, "answer", true);
        room.appendMessage(anonymous);
        QTRY_VERIFY(room.itemWidget(2));
        auto *bare = room.itemWidget(2)->findChild<MessagePreview *>("mamReplyPreview");
        QVERIFY(bare);
        auto *bareTitle = bare->findChild<QLabel *>("previewTitle");
        QVERIFY(bareTitle);
        QVERIFY(bareTitle->text().isEmpty());
        QVERIFY(bareTitle->isHidden());
    }

    void legacyMucReactionsUseBoundedMessageIdWindow()
    {
        // A room without stanza-ids (old ejabberd, no MAM). Twelve messages;
        // rows 9 and 10 reuse the same message@id.
        MamChatWidget room;
        room.setGroupChat(true);
        room.resize(500, 1800);
        room.setHistoryLoading(true);
        const auto base = QDateTime::fromString(QStringLiteral("2026-10-18T10:00:00Z"), Qt::ISODate);
        for (int i = 0; i < 12; ++i) {
            const QString id = (i == 9 || i == 10) ? QStringLiteral("dup") : QStringLiteral("m%1").arg(i);
            auto row = message(id, QStringLiteral("body %1").arg(i), i);
            row.setUserId(QStringLiteral("room@example.org/Peer"));
            room.appendMessage(row);
        }
        room.show();
        for (int i = 0; i < 12; ++i)
            QTRY_VERIFY(room.itemWidget(i));
        const auto legacyKey = [&room](int index) {
            return QStringLiteral("legacy-row:") + room.itemWidget(index)->property("_psiMamRowKey").toString();
        };
        const auto chips = [&room](int index) {
            auto *row = room.itemWidget(index);
            return row ? row->findChildren<QPushButton *>("mamReactionChip").size() : -1;
        };
        const auto incoming = [&](const QString &id, int seconds) {
            MessageView event(MessageView::Reactions);
            event.setReactionsId(id);
            event.setReactions({ QString::fromUtf8("🔥") });
            event.setNick(QStringLiteral("Other"));
            event.setUserId(QStringLiteral("room@example.org/Other"));
            event.setDateTime(base.addSecs(seconds));
            room.appendMessage(event);
        };

        // Sending: message@id goes on the wire, the row key never does.
        QSignalSpy outgoing(&room, &MamChatWidget::outgoingReactions);
        room.toggleReaction(legacyKey(11), QString::fromUtf8("👍"));
        QCOMPARE(outgoing.size(), 1);
        QCOMPARE(outgoing.last().first().toString(), QString("m11"));
        QTRY_COMPARE(chips(11), 1);

        // Row 1 is outside the last ten messages; row 9 is shadowed by the
        // newer row 10 with the same id. Neither may be addressed.
        room.toggleReaction(legacyKey(1), QString::fromUtf8("👍"));
        room.toggleReaction(legacyKey(9), QString::fromUtf8("👍"));
        QCOMPARE(outgoing.size(), 1);
        room.toggleReaction(legacyKey(10), QString::fromUtf8("👍"));
        QCOMPARE(outgoing.size(), 2);
        QCOMPARE(outgoing.last().first().toString(), QString("dup"));

        // Receiving: walk back at most ten messages; the newest match wins.
        incoming(QStringLiteral("m2"), 100);
        QTRY_COMPARE(chips(2), 1);
        incoming(QStringLiteral("m1"), 100);
        incoming(QStringLiteral("dup"), 100);
        QTRY_COMPARE(chips(10), 2); // Our 👍 and their 🔥.
        QCOMPARE(chips(1), 0);
        QCOMPARE(chips(9), 0);

        // Replayed history: messages newer than the reaction do not count.
        incoming(QStringLiteral("m11"), 5);
        incoming(QStringLiteral("m0"), 5);
        QTRY_COMPARE(chips(0), 1);
        QCOMPARE(chips(11), 1);
    }

    void olderArchiveReactionCannotReplaceNewLiveSet()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("original", "hello"));
        MessageView current(MessageView::Reactions);
        current.setReactionsId("original");
        current.setReactions({ QString::fromUtf8("❤️") });
        chat.appendMessage(current);
        auto old = current;
        old.setReactions({ QString::fromUtf8("👍") });
        old.setDateTime(current.dateTime().addSecs(-60));
        old.setSpooled(true);
        chat.appendMessage(old);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        const auto chips = chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip");
        QCOMPARE(chips.size(), 1);
        QVERIFY(chips.first()->text().contains(QString::fromUtf8("❤️")));
    }

    void retractionRequiresTheOriginalSenderAndDoesNotCreateARow()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("original", "visible"));
        chat.show();
        MessageView event(MessageView::MessageRetraction);
        event.setRetractionId("original");
        event.setLocal(true); // Our account cannot retract a peer's message.
        chat.appendMessage(event);
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("visible"));
        event.setLocal(false);
        chat.appendMessage(event);
        QTRY_COMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("Message retracted"));
        QCOMPARE(chat.messageCount(), 1);
        MessageView receipt(MessageView::DeliveryReceipt);
        receipt.setMessageId("original");
        chat.appendMessage(receipt);
        QCOMPARE(chat.messageCount(), 1);
    }

    void deliveryAndDisplayedMarkersUpdateTimestampChecksAndNeverRegress()
    {
        MamChatWidget chat;
        // A large font separates x-height from cap height by several pixels,
        // so a check sized to the line box fails the alignment check below.
        QFont font = chat.font();
        font.setPixelSize(28);
        chat.setFont(font);
        chat.resize(720, 360);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("out-1", "one", 1, true));
        chat.appendMessage(message("out-2", "two", 2, true));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));

        MessageView delivered(MessageView::DeliveryReceipt);
        delivered.setMessageId("out-1");
        chat.appendMessage(delivered);
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("delivered"));
        auto *single = chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator");
        QVERIFY(single->text().isEmpty()); // No font-dependent Unicode ticks.
        const auto *timestamp = chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp");
        const QFontMetrics timestampMetrics(timestamp->font());
        // The icon's widget occupies the complete timestamp line box so the
        // check vertex can be painted at the text baseline inside that box.
        QCOMPARE(single->height(), timestampMetrics.height());
        // Measure the painted ink: the checks stand on the timestamp baseline
        // and reach midway between the x-height and the cap height, so they
        // suit both digit ("22:43") and lowercase ("minutes ago") timestamps.
        const auto inkRows = [](QWidget *widget) {
            const QImage image = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            const qreal  ratio = image.devicePixelRatio();
            int          top = -1, bottom = -1;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) > 64
                        && qGray(image.pixel(x, y)) < qGray(widget->palette().color(QPalette::Window).rgb()) - 40) {
                        if (top < 0)
                            top = y;
                        bottom = y;
                        break;
                    }
            return qMakePair(qRound(top / ratio), qRound(bottom / ratio));
        };
        const int baseline = timestampMetrics.ascent();
        const auto ink = inkRows(single);
        QVERIFY2(ink.first >= 0, "Delivery check painted nothing");
        QVERIFY2(qAbs(ink.second - baseline) <= 1, "Check vertex is not on the timestamp baseline");
        const QFontMetricsF exact(timestamp->font());
        QVERIFY2(qAbs(ink.first - (baseline - (exact.xHeight() + exact.capHeight()) / 2.0)) <= 1,
                 "Check top is not midway between the x-height and the cap height");
        const int singleWidth = single->width();
        QVERIFY(singleWidth > 0);
        QCOMPARE(single->palette().color(QPalette::WindowText),
                 chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp")->palette().color(QPalette::WindowText));
        QVERIFY(!chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator"));

        MessageView read(MessageView::DisplayedMarker);
        read.setMessageId("out-2");
        chat.appendMessage(read);
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("read"));
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator")->toolTip(),
                     QString("Read"));
        auto *doubleChecks = chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator");
        QVERIFY(doubleChecks->text().isEmpty());
        QCOMPARE(doubleChecks->height(), timestampMetrics.height());
        QCOMPARE(inkRows(doubleChecks), ink); // Both states share one vertical alignment.
        // What the user sees: in the laid-out timestamp group, the checks
        // occupy the same rows as lowercase timestamp text. Psi derives the
        // timestamp font from the chat font in points, whose fractional
        // metrics shifted the checks by a pixel, so try several sizes.
        const auto alignedWithText = [&chat](const QFont &chatFont) {
            chat.setFont(chatFont);
            auto *row   = chat.itemWidget(1);
            auto *time  = row->findChild<QLabel *>("mamTimestamp");
            auto *marks = row->findChild<QLabel *>("mamDeliveryIndicator");
            auto *group = time->parentWidget();
            const auto rows = [group](const QString &text, QWidget *widget) {
                qobject_cast<QLabel *>(group->findChild<QLabel *>("mamTimestamp"))->setText(text);
                group->layout()->activate();
                const QImage image      = group->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                const qreal  ratio      = image.devicePixelRatio();
                const int    background = qGray(image.pixel(0, 0)); // above lowercase text
                const QRect  rect       = widget->geometry();
                int          top = -1, bottom = -1;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = qRound(rect.left() * ratio); x <= qRound(rect.right() * ratio) && x < image.width(); ++x)
                        if (qAbs(qGray(image.pixel(x, y)) - background) > 48) {
                            if (top < 0)
                                top = y;
                            bottom = y;
                            break;
                        }
                return qMakePair(top, bottom);
            };
            const auto lower  = rows(QStringLiteral("xxxx"), time);
            const auto digits = rows(QStringLiteral("0000"), time);
            const auto checks = rows(QStringLiteral("xxxx"), marks);
            // On the baseline, no taller than digits, at least as tall as lowercase.
            if (lower.first < 0 || digits.first < 0 || checks.first < 0 || checks.second != lower.second
                || checks.first < digits.first - 1 || checks.first > lower.first + 1) {
                qWarning() << chatFont << "lowercase rows" << lower << "digit rows" << digits << "check rows" << checks;
                return false;
            }
            return true;
        };
        for (const qreal points : { 8.0, 9.0, 10.0, 11.0, 12.5, 14.0 }) {
            QFont pointFont = font;
            pointFont.setPointSizeF(points);
            QVERIFY2(alignedWithText(pointFont), "Checks are not aligned with the timestamp text");
        }
        QVERIFY2(alignedWithText(font), "Checks are not aligned with the timestamp text");
        QVERIFY(doubleChecks->width() > singleWidth);
        QVERIFY(doubleChecks->width() < singleWidth * 2); // Overlap, not two separate glyphs.

        read.setMessageId("out-1"); // A stale marker may not roll back the cursor.
        chat.appendMessage(read);
        QCOMPARE(chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                 QString("read"));

        // A history prepend shifts indices, not the semantic read cursor.
        auto older = message("out-0", "older", 0, true);
        older.setHistoryId(10);
        chat.prependArchivePage({ qMakePair(QString("older-archive"), older) });
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("read"));
    }

    void archiveReadMarkerIsAppliedWhenTargetArrivesOnAnOlderPage()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        MessageView marker(MessageView::DisplayedMarker);
        marker.setMessageId("archived-out");
        chat.appendMessage(marker); // Newest MAM page contains marker, not target.
        QCOMPARE(chat.messageCount(), 0);

        auto original = message("archived-out", "older outgoing", 0, true);
        original.setHistoryId(10);
        chat.prependArchivePage({ qMakePair(QString("older-mam-id"), original) });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                 QString("read"));
        QCOMPARE(chat.messageCount(), 1);
    }

    void visibleMarkableIncomingMessagesAdvanceCursorOnlyOnce()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        auto first = message("in-1", "first", 1);
        first.setMarkable(true);
        chat.appendMessage(first);
        auto second = message("in-2", "second", 2);
        second.setMarkable(true);
        chat.appendMessage(second);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("in-2"));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
        // Receiving a delivery event never affects the outgoing read cursor.
        MessageView receipt(MessageView::DeliveryReceipt);
        receipt.setMessageId("in-1");
        chat.appendMessage(receipt);
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
    }

    void savedOwnDisplayedMarkerPreventsResendingAfterReopen_data()
    {
        QTest::addColumn<bool>("markerFirst");
        QTest::newRow("marker-before-messages") << true;
        QTest::newRow("marker-after-messages") << false;
    }
    void savedOwnDisplayedMarkerPreventsResendingAfterReopen()
    {
        QFETCH(bool, markerFirst);
        for (int reopening = 0; reopening < 2; ++reopening) {
            MamChatWidget chat;
            chat.resize(520, 320);
            chat.setHistoryLoading(true);
            auto first = message("in-1", "first", 1);
            first.setMarkable(true);
            auto second = message("in-2", "second", 2);
            second.setMarkable(true);
            MessageView marker(MessageView::DisplayedMarker);
            marker.setLocal(true);
            marker.setSpooled(true);
            marker.setMessageId("in-2");
            if (markerFirst)
                chat.appendMessage(marker);
            chat.prependArchivePage({ { "archive-1", first }, { "archive-2", second } });
            if (!markerFirst)
                chat.appendMessage(marker);
            chat.show();
            QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
            QCOMPARE(chat.messageCount(), 2); // The marker is never a row.
            QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());

            auto next = message("in-3", "new unread message", 3);
            next.setMarkable(true);
            chat.appendMessage(next);
            QTRY_VERIFY(chat.itemWidget(2));
            QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("in-3"));
            marker.setMessageId("in-1"); // Older replay must not move backwards.
            chat.appendMessage(marker);
            QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
        }
    }

    void persistentDisplayedCursorCoversPagesBeforeItsUnloadedTarget()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        const auto time = message("anchor", "read already", 3).dateTime();
        chat.restoreDisplayedIncoming("anchor", time, 30);
        auto older = message("older", "older page", 1);
        older.setMarkable(true);
        older.setHistoryId(10);
        chat.prependArchivePage({ { "older-uid", older } });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());

        auto sameTimeOlder = message("earlier-at-same-time", "same timestamp, older event", 3);
        sameTimeOlder.setMarkable(true);
        sameTimeOlder.setHistoryId(29);
        chat.appendMessage(sameTimeOlder);
        QTRY_VERIFY(chat.itemWidget(1));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());

        auto newer = message("newer", "same timestamp, newer event", 3);
        newer.setMarkable(true);
        newer.setHistoryId(31);
        chat.appendMessage(newer);
        // The insert sorts before another row at the same timestamp. Its
        // existing neighbour can be materialized before the new row is ready.
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->property("_psiMamRowKey").toString() == "edb:31");
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("newer"));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());

        auto next = message("next", "another message at the same time", 3);
        next.setMarkable(true);
        next.setHistoryId(32);
        chat.appendMessage(next);
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->property("_psiMamRowKey").toString() == "edb:32");
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("next"));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());

        chat.clearMessages();
        chat.appendMessage(older);
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("older"));
    }

    void storedMessageMaskNeedsNoProtocolEventsInTheVisiblePage()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        auto incoming = message("read-incoming", "already read", 1);
        incoming.setMarkable(true);
        incoming.setHistoryId(10);
        incoming.setMessageState(MessageState::Displayed);
        auto outgoing = message("edited-outgoing", "retracted original text", 2, true);
        outgoing.setHistoryId(20);
        outgoing.setMessageState(MessageState::Displayed | MessageState::Delivered | MessageState::Edited | MessageState::Retracted);
        chat.prependArchivePage({ { "incoming-uid", incoming }, { "outgoing-uid", outgoing } });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
        const auto verifyOutgoing = [&chat] {
            auto *row = chat.itemWidget(1);
            QVERIFY(row);
            auto *indicator = row->findChild<QWidget *>("mamDeliveryIndicator");
            QVERIFY(indicator);
            QCOMPARE(indicator->property("deliveryState").toString(), QString("read"));
            QVERIFY(row->findChild<QWidget *>("mamEditedIndicator"));
            auto *body = row->findChild<ChatMessageText *>();
            QVERIFY(body);
            QCOMPARE(body->toPlainText(), QString("Message retracted"));
        };
        verifyOutgoing();
        chat.selectMessageRange(1, 1);
        chat.copySelectedMessages();
        QVERIFY(!QApplication::clipboard()->text().contains("retracted original text"));
        chat.cancelMessageSelection();

        for (int i = 0; i < 150; ++i)
            chat.appendMessage(message(QString::number(i), "newer message", i + 3));
        chat.scrollToEnd();
        QTRY_VERIFY(!chat.itemWidget(1));
        chat.scrollToIndex(0, Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
        verifyOutgoing(); // Recreated QWidgets inherit state from the message.
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
    }

    void staleRevisionCannotResurrectAStoredRetraction()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        auto original = message("original", "hidden text", 1);
        original.setHistoryId(10);
        original.setMessageState(MessageState::Retracted);
        chat.prependArchivePage({ { "original-uid", original } });
        auto edit = message("edit", "also hidden", 2);
        edit.setReplaceId("original");
        chat.appendMessage(edit); // Live events have no persisted-state mask.
        chat.show();
        QCOMPARE(chat.messageCount(), 1);
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QCOMPARE(body->toPlainText(), QString("Message retracted"));
        QVERIFY(chat.itemWidget(0)->findChild<QWidget *>("mamEditedIndicator"));
    }

    void directLinkClickOpensOnceWithoutStealingDragSelection()
    {
        ChatMessageText body;
        body.resize(450, 80);
        body.setRichText(QStringLiteral("Before <a href='https://example.org/path'>click here</a> after"));
        body.show();
        QTRY_VERIFY(body.viewport()->isVisible());

        const QString href = QStringLiteral("https://example.org/path");
        QTextCursor cursor = body.document()->find(QStringLiteral("click here"));
        QVERIFY(!cursor.isNull());
        cursor.setPosition(cursor.selectionStart() + 3); // Inside the anchor, not at its right edge.
        const QPoint link = body.cursorRect(cursor).center();
        QCOMPARE(body.anchorAt(link), href);
        QSignalSpy opened(URLObject::getInstance(), &URLObject::openURL);
        QVERIFY(opened.isValid());

        QTest::mouseClick(body.viewport(), Qt::LeftButton, Qt::NoModifier, link);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.takeFirst().first().toString(), href);

        // An ordinary drag beginning on an anchor selects text, not a URL.
        const QPoint beyond = link + QPoint(QApplication::startDragDistance() + 35, 0);
        QTest::mousePress(body.viewport(), Qt::LeftButton, Qt::NoModifier, link);
        QTest::mouseMove(body.viewport(), beyond);
        QTest::mouseRelease(body.viewport(), Qt::LeftButton, Qt::NoModifier, beyond);
        QCOMPARE(opened.size(), 0);

        // A whole-message selection replaces the text interaction mode.
        body.setWholeMessageSelectionMode(true);
        QTest::mouseClick(body.viewport(), Qt::LeftButton, Qt::NoModifier, link);
        QCOMPARE(opened.size(), 0);
    }

    void linkContextMenuSelectsLinkFillingWholeMessage()
    {
        // A message that is only a link: the anchor touches both ends of the
        // per-message document, where cursor movement stops succeeding.
        ChatMessageText body;
        body.resize(400, 80);
        body.setRichText(QStringLiteral("<a href=\"mailto:user@example.org\">user@example.org</a>"));
        body.show();
        QVERIFY(QTest::qWaitForWindowExposed(&body));
        const QString link = QStringLiteral("user@example.org");
        for (const int at : { 1, int(link.size()) - 1 }) {
            QTextCursor probe(body.document());
            probe.setPosition(at);
            const QPoint position = body.cursorRect(probe).center();
            QVERIFY(!body.anchorAt(position).isEmpty());
            std::unique_ptr<QMenu> menu(body.createStandardContextMenu(position));
            QVERIFY(menu);
            QCOMPARE(body.textCursor().selectedText(), link);
        }
    }

    void largeSimsImageGetsItsOwnParagraphEvenWithCaption()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(240, 140, QImage::Format_RGB32);
        image.preview_.fill(Qt::green);
        opener.items.insert(QStringLiteral("photo"), &image);

        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.setRichText(QStringLiteral("Short comment <share id=\"photo\" account=\"account-a\"/> continued"));
        const auto doc = body.document();
        QCOMPARE(doc->blockCount(), 3);
        QCOMPARE(doc->begin().text(), QStringLiteral("Short comment "));
        const auto media = doc->begin().next();
        QCOMPARE(media.text(), QString(QChar::ObjectReplacementCharacter));
        QVERIFY(media.begin().fragment().charFormat().isImageFormat());
        QCOMPARE(media.begin().fragment().charFormat().toImageFormat().name(), QStringLiteral("share:photo"));
        QCOMPARE(media.next().text(), QStringLiteral(" continued"));
        QVERIFY(body.heightForWidth(500) >= 140 + 2 * body.fontMetrics().height());
    }

    void multipleLargeMediaObjectsGetIndependentParagraphs()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem first(QStringLiteral("image/png"));
        FileSharingItem second(QStringLiteral("image/png"));
        first.preview_ = QImage(240, 140, QImage::Format_RGB32);
        second.preview_ = QImage(180, 120, QImage::Format_RGB32);
        first.preview_.fill(Qt::green);
        second.preview_.fill(Qt::blue);
        opener.items.insert(QStringLiteral("first"), &first);
        opener.items.insert(QStringLiteral("second"), &second);

        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.setRichText(QStringLiteral("Before <share id=\"first\" account=\"account-a\"/>"
                                        "<share id=\"second\" account=\"account-a\"/> after"));
        const QTextDocument *doc = body.document();
        QCOMPARE(doc->blockCount(), 4);
        QCOMPARE(doc->begin().text(), QStringLiteral("Before "));
        const auto firstBlock = doc->begin().next();
        const auto secondBlock = firstBlock.next();
        QCOMPARE(firstBlock.text(), QString(QChar::ObjectReplacementCharacter));
        QCOMPARE(secondBlock.text(), QString(QChar::ObjectReplacementCharacter));
        QCOMPARE(firstBlock.begin().fragment().charFormat().toImageFormat().name(),
                 QStringLiteral("share:first"));
        QCOMPARE(secondBlock.begin().fragment().charFormat().toImageFormat().name(),
                 QStringLiteral("share:second"));
        QCOMPARE(secondBlock.next().text(), QStringLiteral(" after"));
    }

    void smallInlineMediaStaysWithTextAtDoubleFontThreshold()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(12, 12, QImage::Format_RGB32);
        image.preview_.fill(Qt::red);
        opener.items.insert(QStringLiteral("tiny"), &image);

        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.setRichText(QStringLiteral("Before <share id=\"tiny\" account=\"account-a\"/> after"));
        QCOMPARE(body.document()->blockCount(), 1);
        const auto text = body.document()->begin().text();
        QVERIFY(text.startsWith(QStringLiteral("Before ")));
        QVERIFY(text.contains(QChar::ObjectReplacementCharacter));
        QVERIFY(text.endsWith(QStringLiteral(" after")));
    }

    void mediaInsertedAfterInitialRenderingAlsoGetsParagraph()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(12, 12, QImage::Format_RGB32);
        image.preview_.fill(Qt::red);
        opener.items.insert(QStringLiteral("tiny"), &image);

        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.setRichText(QStringLiteral("Prefix <share id=\"tiny\" account=\"account-a\"/> suffix"));
        QCOMPARE(body.document()->blockCount(), 1);

        // Like PsiTextView's async download callback, replace an existing inline
        // image object with a bigger decoded preview after initial layout.
        const auto first = body.document()->begin().text();
        const int at = first.indexOf(QChar::ObjectReplacementCharacter);
        QVERIFY(at >= 0);
        QTextCursor cursor(body.document());
        cursor.setPosition(at);
        cursor.setPosition(at + 1, QTextCursor::KeepAnchor);
        QTextImageFormat largeImage;
        largeImage.setName(QStringLiteral("async-large"));
        largeImage.setWidth(260);
        largeImage.setHeight(150);
        cursor.insertImage(largeImage);

        QTRY_COMPARE(body.document()->blockCount(), 3);
        QCOMPARE(body.document()->begin().text(), QStringLiteral("Prefix "));
        QCOMPARE(body.document()->begin().next().text(), QString(QChar::ObjectReplacementCharacter));
        QCOMPARE(body.document()->begin().next().next().text(), QStringLiteral(" suffix"));
    }

    void simsMediaUsesExistingPsiTextViewObjects()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem video(QStringLiteral("video/webm"));
        FileSharingItem audio(QStringLiteral("audio/ogg"));
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(60,40,QImage::Format_RGB32);
        image.preview_.fill(Qt::green);
        opener.items.insert(QStringLiteral("video"),&video);
        opener.items.insert(QStringLiteral("audio"),&audio);
        opener.items.insert(QStringLiteral("image"),&image);
        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.resize(600,450);
        body.setRichText(QStringLiteral("Before <share id=\"video\" account=\"account-a\"/> between "
                                        "<share id=\"audio\" account=\"account-a\"/> "
                                        "<share id=\"image\" account=\"account-a\"/> after"));
        body.show();
        int videoObjects=0,audioObjects=0,imageObjects=0;
        for (auto block=body.document()->begin();block.isValid();block=block.next())
            for (auto it=block.begin();!it.atEnd();++it) {
                const auto format=it.fragment().charFormat();
                if (format.isImageFormat()) {
                    ++imageObjects;
                    continue;
                }
                auto *handler=body.document()->documentLayout()->handlerForObject(format.objectType());
                if (dynamic_cast<ITEVideoController*>(handler)) ++videoObjects;
                else if (dynamic_cast<ITEAudioController*>(handler)) ++audioObjects;
                else continue;
                bool sourceFound=false;
                for (const auto &property:format.properties()) {
                    if (property.userType()!=qMetaTypeId<QUrl>()) continue;
                    const auto url=property.toUrl();
                    QCOMPARE(url.scheme(),QStringLiteral("share"));
                    QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("account")),QStringLiteral("account-a"));
                    sourceFound=true;
                }
                QVERIFY(sourceFound);
            }
        QCOMPARE(videoObjects,1);
        QCOMPARE(audioObjects,1);
        QCOMPARE(imageObjects,1);
        QVERIFY(body.heightForWidth(600)>=220);
        QVERIFY(body.toPlainText().contains(QStringLiteral("Before")));
        QVERIFY(body.toPlainText().contains(QStringLiteral("after")));
        body.setRichText(QStringLiteral("Replaced"));
        QCoreApplication::processEvents(); // Deferred qite cursor/player cleanup.
        QCOMPARE(body.toPlainText(),QStringLiteral("Replaced"));
    }
    void simsGenericFilesUsePreviewActionsWithoutChangingMediaRendering()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem archive(QStringLiteral("application/x-7z-compressed"));
        FileSharingItem video(QStringLiteral("video/webm"));
        opener.items.insert(QStringLiteral("archive"), &archive);
        opener.items.insert(QStringLiteral("video"), &video);

        MamChatWidget list;
        list.resize(550, 400);
        list.setHistoryLoading(true);
        list.setMediaOpener(&opener);
        list.appendMessage(message(QStringLiteral("file"), QStringLiteral(
            "Before <share id=\"archive\" account=\"account-a\"/> and "
            "<share id=\"video\" account=\"account-a\"/> after")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *row = list.itemWidget(0);
        auto *file = row->findChild<MessagePreview *>(QStringLiteral("mamFilePreview"));
        QVERIFY(file);
        auto *title = file->findChild<QLabel *>(QStringLiteral("previewTitle"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("test.7z"));
        auto *body = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QVERIFY(body);
        QVERIFY(body->toPlainText().contains(QStringLiteral("Before")));
        QVERIFY(body->toPlainText().contains(QStringLiteral("after")));
        QVERIFY(body->toPlainText().contains(QChar::ObjectReplacementCharacter)); // Video still inline.
        auto buttons = file->findChildren<QToolButton *>();
        QToolButton *download = nullptr;
        QToolButton *open = nullptr;
        QToolButton *reveal = nullptr;
        for (auto *button : buttons) {
            if (button->toolTip() == QStringLiteral("Download file")) download = button;
            if (button->toolTip() == QStringLiteral("Open file")) open = button;
            if (button->toolTip() == QStringLiteral("Show in file manager")) reveal = button;
        }
        QVERIFY(download);
        QVERIFY(open);
        QVERIFY(reveal);
        QVERIFY(!reveal->isEnabled());
        download->click();
        QCOMPARE(opener.saves, 1);
        QVERIFY(!download->isEnabled());
        auto *status = file->findChild<QLabel *>(QStringLiteral("mamFileStatus"));
        QVERIFY(status);
        QCOMPARE(status->text(), QStringLiteral("Downloading…"));

        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        list.setTheme(classic);
        QTRY_VERIFY(list.itemWidget(0));
        QCOMPARE(list.itemWidget(0)->findChildren<MessagePreview *>(QStringLiteral("mamFilePreview")).size(), 1);
    }

    void sharesFollowTheAutomaticDownloadLimit()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem unasked(QStringLiteral("image/png")); // e.g. from a stranger
        unasked.cached_ = false;
        FileSharingItem allowed(QStringLiteral("image/png")); // within the message's limit
        allowed.cached_ = false;
        allowed.downloadsItself_ = true;
        FileSharingItem bare(QStringLiteral("application/pdf")); // a bare OOB URL, not checked yet
        bare.cached_ = false;
        bare.urlOnly_ = true;
        opener.items.insert(QStringLiteral("unasked"), &unasked);
        opener.items.insert(QStringLiteral("allowed"), &allowed);
        opener.items.insert(QStringLiteral("bare"), &bare);

        MamChatWidget list;
        list.resize(550, 700);
        list.setHistoryLoading(true);
        list.setMediaOpener(&opener);
        list.appendMessage(message(QStringLiteral("limits"), QStringLiteral(
            "<share id=\"unasked\" account=\"account-a\"/> "
            "<share id=\"allowed\" account=\"account-a\" auto=\"1048576\"/> "
            "<share id=\"bare\" account=\"account-a\" auto=\"1048576\"/>")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        const auto cards = [&list] {
            return list.itemWidget(0)->findChildren<MessagePreview *>(QStringLiteral("mamFilePreview")).size();
        };
        QCOMPARE(cards(), 2); // The image that downloads by itself stays in the text.
        QCOMPARE(allowed.autoLimits_.size(), 1);
        QCOMPARE(allowed.autoLimits_.first(), quint64(1048576));
        QVERIFY(unasked.autoLimits_.isEmpty());
        QCOMPARE(bare.remoteChecks_, 1);
        int browserButtons = 0;
        for (auto *button : list.itemWidget(0)->findChildren<QToolButton *>())
            browserButtons += button->toolTip() == QStringLiteral("Open in browser");
        QCOMPARE(browserButtons, 2);

        // The server's answer builds the row again, without asking twice.
        QPointer<QWidget> row = list.itemWidget(0);
        bare.remoteChecked_ = true;
        emit bare.remoteChecked();
        QTRY_VERIFY(!row);
        QTRY_VERIFY(list.itemWidget(0));
        QCOMPARE(bare.remoteChecks_, 1);
        QCOMPARE(cards(), 2);

        // An image that fails to download by itself becomes a card.
        row = list.itemWidget(0);
        allowed.downloadsItself_ = false;
        emit allowed.downloadFinished();
        QTRY_VERIFY(!row);
        QTRY_VERIFY(list.itemWidget(0));
        QCOMPARE(cards(), 3);
    }

    void virtualRowsReceiveMediaOpenerAndRecreateAttachments()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem video(QStringLiteral("video/webm"));
        opener.items.insert(QStringLiteral("video"),&video);
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.setMediaOpener(&opener);
        list.resize(550,400);
        list.appendMessage(message(QStringLiteral("sims"),QStringLiteral("<share id=\"video\" account=\"account-a\"/>")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *body=list.itemWidget(0)->findChild<ChatMessageText*>();
        QVERIFY(body->document()->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QVERIFY(body->heightForWidth(500)>=220);
        NativeChatTheme theme;
        theme.layout=NativeChatTheme::Classic;
        list.setTheme(theme); // Recreate materialized row with the same attachment.
        QTRY_VERIFY(list.itemWidget(0));
        body=list.itemWidget(0)->findChild<ChatMessageText*>();
        QVERIFY(body->document()->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QCOMPARE(opener.opens,0); // Merely displaying video never opens a decoder/source.
    }
    void archivePrependKeepsAtEndUntilUserScrollsUp()
    {
        MamChatWidget list;
        list.resize(400, 220);
        list.setHistoryLoading(true);
        list.show();
        QList<QPair<QString, MessageView>> page;
        for (int i = 0; i < 35; ++i) {
            auto entry = message(QString::number(i), QStringLiteral("History"), i);
            entry.setHistoryId(i + 1);
            page.append(qMakePair(QString(), entry));
        }
        QCOMPARE(list.prependArchivePage(page), 35);
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        QVERIFY(list.isAtEnd());
        list.appendMessage(message(QStringLiteral("incoming"), QStringLiteral("Live"), 36));
        QVERIFY(list.isAtEnd());
        // A real scrollbar action (unlike geometry restoration) releases the
        // opening-at-end behavior before another MAM page arrives.
        list.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QVERIFY(!list.isAtEnd());
        auto older = message(QStringLiteral("old"), QStringLiteral("Older"), -10);
        older.setHistoryId(101);
        list.prependArchivePage({qMakePair(QString(), older)});
        QVERIFY(!list.isAtEnd());
    }

    void openingRoomWhileHiddenFollowsTailUntilUserScroll()
    {
        MamChatWidget room;
        room.setGroupChat(true);
        room.setHistoryLoading(true);
        room.resize(400, 210);
        // MUC presence/status events and local history can arrive BEFORE show().
        for (int i = 0; i < 42; ++i)
            room.appendMessage(message(QStringLiteral("status-%1").arg(i),
                                       QStringLiteral("Room message"), i));
        room.show();
        QTRY_VERIFY(room.verticalScrollBar()->maximum() > 0);
        QTRY_VERIFY(room.isAtEnd());
        room.resize(480, 180); // Initial splitter and font geometry settle.
        QTRY_VERIFY(room.isAtEnd());

        QList<QPair<QString, MessageView>> page;
        for (int i = 0; i < 20; ++i) {
            auto historical = message(QStringLiteral("arch-%1").arg(i),
                                      QStringLiteral("Archived"), -100 + i);
            page.append(qMakePair(QStringLiteral("mam-%1").arg(i), historical));
        }
        QCOMPARE(room.prependArchivePage(page), 20);
        QTRY_VERIFY(room.isAtEnd());

        QSignalSpy userScroll(&room, &Mattermost::LongListWidget::userScrollStarted);
        room.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QTRY_VERIFY(userScroll.count() > 0);
        QVERIFY(!room.isAtEnd());
        room.appendMessage(message(QStringLiteral("live-after-scroll"),
                                   QStringLiteral("Do not steal the viewport"), 100));
        QVERIFY(!room.isAtEnd());

        room.clearMessages();
        for (int i = 0; i < 25; ++i)
            room.appendMessage(message(QStringLiteral("new-%1").arg(i),
                                       QStringLiteral("Second conversation"), i));
        QTRY_VERIFY(room.isAtEnd());
        room.scrollToIndex(0, Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(!room.isAtEnd());
        room.appendMessage(message(QStringLiteral("new-tail"),
                                   QStringLiteral("Explicit navigation remains in place"), 50));
        QVERIFY(!room.isAtEnd());
    }

    void timeFontColorAndClassicNicknamePaletteAreStable()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        QFont configured = list.font();
        configured.setPointSize(12);
        list.setFont(configured);
        list.resize(500, 280);
        list.setHistoryLoading(true);
        list.setNicknameColorResolver([](const MessageView &) { return QColor(60, 95, 150); });
        list.appendMessage(message(QStringLiteral("time"), QStringLiteral("Time style")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto timestamp = [&]() -> QLabel * {
            return list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        };
        QCOMPARE(timestamp()->font().pointSizeF(), 11.0);
        QCOMPARE(timestamp()->palette().color(QPalette::Text).alpha(), 188);

        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        // Classic has the time in the message text, at the chat font size and
        // in the nickname colour in group chats.
        const auto classicTime = [&]() -> QTextCharFormat {
            auto *body = list.itemWidget(0)->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
            for (auto it = body->document()->begin().begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().anchorNames().contains(QStringLiteral("psi-time")))
                    return it.fragment().charFormat();
            return {};
        };
        QTRY_VERIFY(list.itemWidget(0) && !timestamp() && classicTime().isValid());
        QCOMPARE(classicTime().font().pointSizeF(), 12.0);
        QCOMPARE(classicTime().foreground().color(), QColor(60, 95, 150));
        NativeChatTheme modern;
        modern.layout = NativeChatTheme::Modern;
        modern.avatarSize = 40;
        list.setTheme(modern);
        QTRY_VERIFY(timestamp() && qFuzzyCompare(timestamp()->font().pointSizeF(), 11.0));
        QCOMPARE(timestamp()->palette().color(QPalette::Text).alpha(), 188);
        list.setTheme(classic);
        QTRY_VERIFY(list.itemWidget(0) && !timestamp() && classicTime().isValid());
        list.setTheme(modern);
        QTRY_VERIFY(timestamp() && qFuzzyCompare(timestamp()->font().pointSizeF(), 11.0));
    }

    void pluginRendererArrangesPsiParts()
    {
        FramedRenderer renderer;
        MamChatWidget  list;
        list.resize(700, 500);
        list.setHistoryLoading(true);
        list.setTheme({ QStringLiteral("org.example.framed"), QStringLiteral("Framed"), NativeChatTheme::Modern, 40, 10, {} },
                      &renderer);
        auto incoming = message(QStringLiteral("in"), QStringLiteral("Incoming text"));
        incoming.setReactionTargetId(QStringLiteral("in-origin"));
        list.appendMessage(incoming);
        list.appendMessage(message(QStringLiteral("out"), QStringLiteral("Own text"), 1, true));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));

        auto *row    = list.itemWidget(0);
        auto *frame  = row->findChild<QFrame *>(QStringLiteral("pluginFrame"));
        auto *body   = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QVERIFY(frame && body && author);
        QVERIFY(frame->isAncestorOf(body) && frame->isAncestorOf(author));
        QCOMPARE(author->text(), QStringLiteral("Peer says"));
        QVERIFY(!author->font().bold());
        QVERIFY(body->mapTo(row, QPoint()).y() < author->mapTo(row, QPoint()).y());
        // Parts the renderer left out exist but stay hidden.
        auto *time   = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        auto *avatar = row->findChild<QLabel *>(QStringLiteral("mamAvatar"));
        QVERIFY(time && avatar);
        QVERIFY(time->isHidden() && avatar->isHidden());
        QCOMPARE(avatar->width(), 24);
        const QString clock = incoming.dateTime().toLocalTime().toString(QStringLiteral("HH:mm"));
        QCOMPARE(time->text(), clock);
        // A clock time needs no relative-time refresh.
        QVERIFY(!list.findChild<QTimer *>(QStringLiteral("mamTimestampRefresh"))->isActive());

        // Body colour override for our own message only.
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText *>()->palette().color(QPalette::Text), QColor(200, 0, 0));
        QVERIFY(body->palette().color(QPalette::Text) != QColor(200, 0, 0));

        // Hover actions float at the top-right corner of the anchor.
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(row, &enter);
        auto *bar = list.findChild<QFrame *>(QStringLiteral("mamMessageActions"));
        QTRY_VERIFY(bar->isVisible());
        const QPoint frameTopRight = frame->mapTo(list.viewport(), QPoint(frame->width(), 0));
        QCOMPARE(bar->x() + bar->width() + 8, frameTopRight.x());
        QVERIFY(frameTopRight.x() < row->width() - 100);

        // Palette changes restyle rows in place and give the renderer its turn.
        const int restyledBefore = renderer.restyled;
        QPalette  palette        = list.palette();
        palette.setColor(QPalette::Text, QColor(10, 20, 30));
        list.setPalette(palette);
        QTRY_VERIFY(renderer.restyled > restyledBefore);
        QCOMPARE(list.itemWidget(0), row);
        QVERIFY(row->property("restyled").toInt() > 0);
        QCOMPARE(author->palette().color(QPalette::Text), QColor(10, 20, 30));

        // Switching to a description theme drops the renderer for new rows.
        list.setTheme({ QStringLiteral("modern"), QStringLiteral("Modern"), NativeChatTheme::Modern, 40, 10, {} });
        QTRY_VERIFY(list.itemWidget(0) && !list.itemWidget(0)->findChild<QFrame *>(QStringLiteral("pluginFrame")));
        const int restyledAfterSwitch = renderer.restyled;
        palette.setColor(QPalette::Text, QColor(30, 20, 10));
        list.setPalette(palette);
        QCoreApplication::processEvents();
        QCOMPARE(renderer.restyled, restyledAfterSwitch);
    }

    void bubbleThemeAlignsOwnMessagesAndLimitsWidth()
    {
        MamChatWidget list;
        list.resize(1300, 700);
        list.setHistoryLoading(true);
        list.setTheme({ QStringLiteral("bubble"), QStringLiteral("Bubble"), NativeChatTheme::Modern, 40, 6, {} },
                      NativeChatRenderers::bubble());
        auto incoming = message(QStringLiteral("short-in"), QStringLiteral("Hi"));
        incoming.setReactionTargetId(QStringLiteral("short-in-origin"));
        list.appendMessage(incoming);
        auto own = message(QStringLiteral("short-out"), QStringLiteral("Hello"), 1, true);
        own.setReactionTargetId(QStringLiteral("short-out-origin"));
        list.appendMessage(own);
        list.appendMessage(message(QStringLiteral("long"), QStringLiteral("A long line of text. ").repeated(30), 2));
        MessageView notice(MessageView::Status);
        notice.setPlainText(QStringLiteral("Peer is away"));
        list.appendMessage(notice);
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2) && list.itemWidget(3));

        const auto parts = [&list](int index) {
            auto *row = list.itemWidget(index);
            return std::make_tuple(row, row->findChild<QWidget *>(QStringLiteral("mamBubble")),
                                   row->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        };
        auto [inRow, inBubble, inAvatar] = parts(0);
        QVERIFY(inBubble && inAvatar);
        QCOMPARE(inBubble->property("tailSide").toString(), QStringLiteral("left"));
        QVERIFY(inAvatar->mapTo(inRow, QPoint()).x() < inBubble->mapTo(inRow, QPoint()).x());
        QVERIFY(inBubble->mapTo(inRow, QPoint()).x() < inRow->width() / 4);
        QCOMPARE(inBubble->mapTo(inRow, QPoint()).y(), inAvatar->mapTo(inRow, QPoint()).y());
        // A short message gets a small bubble, not a full-width one.
        QVERIFY(inBubble->width() < inRow->width() / 3);

        auto [outRow, outBubble, outAvatar] = parts(1);
        QVERIFY(outBubble && outAvatar);
        QCOMPARE(outBubble->property("tailSide").toString(), QStringLiteral("right"));
        QVERIFY(outAvatar->mapTo(outRow, QPoint()).x() > outBubble->mapTo(outRow, QPoint()).x());
        QVERIFY(outBubble->mapTo(outRow, QPoint(outBubble->width(), 0)).x() > outRow->width() * 3 / 4);
        QVERIFY(outBubble->width() < outRow->width() / 3);

        // A long message is limited to about 60 characters of the chat font,
        // whatever the width of the log.
        auto [longRow, longBubble, longAvatar] = parts(2);
        QVERIFY(longBubble);
        const int textLimit = qCeil(QFontMetricsF(longBubble->font()).averageCharWidth()
                                    * NativeChatRenderers::BubbleMaxTextColumns);
        const QMargins margins = longBubble->layout()->contentsMargins();
        QCOMPARE(longBubble->maximumWidth(), textLimit + margins.left() + margins.right());
        QCOMPARE(longBubble->width(), longBubble->maximumWidth());
        QVERIFY(longBubble->width() < longRow->width() - 200);
        auto *longBody = longRow->findChild<ChatMessageText *>();
        QVERIFY(longBody->width() <= textLimit);
        QVERIFY(longBody->height() > 2 * longBody->fontMetrics().height()); // Wrapped.
        list.resize(1600, 700);
        QTRY_COMPARE(list.itemWidget(2)->findChild<QWidget *>(QStringLiteral("mamBubble"))->width(),
                     longBubble->maximumWidth());

        // Notices are centred bubbles without a tail, shrunk to their text.
        auto *noticeRow    = list.itemWidget(3);
        auto *noticeBubble = noticeRow->findChild<QWidget *>(QStringLiteral("mamBubble"));
        QVERIFY(noticeBubble);
        QCOMPARE(noticeBubble->property("tailSide").toString(), QStringLiteral("none"));
        QVERIFY(!noticeRow->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        const int noticeCentre = noticeBubble->mapTo(noticeRow, QPoint(noticeBubble->width() / 2, 0)).x();
        QVERIFY(qAbs(noticeCentre - noticeRow->width() / 2) <= 2);
        QVERIFY(noticeBubble->width() < noticeRow->width() / 3);

        // The hover actions sit on the bubble, not on the far end of the row.
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(inRow, &enter);
        auto *bar = list.findChild<QFrame *>(QStringLiteral("mamMessageActions"));
        QTRY_VERIFY(bar->isVisible());
        QCOMPARE(bar->x() + bar->width() + 8, inBubble->mapTo(list.viewport(), QPoint(inBubble->width(), 0)).x());

        // A larger chat font widens the limit with it.
        const int before = longBubble->maximumWidth();
        QFont     larger = list.font();
        larger.setPointSizeF(larger.pointSizeF() > 0 ? larger.pointSizeF() + 4 : 16);
        list.setFont(larger);
        QTRY_VERIFY(list.itemWidget(2)->findChild<QWidget *>(QStringLiteral("mamBubble"))->maximumWidth() > before);
    }

    void styleSheetColoursOverridePsiColours_data()
    {
        QTest::addColumn<bool>("bubble");
        QTest::addColumn<bool>("themeSheet");
        QTest::newRow("modern, chat window sheet") << false << false;
        QTest::newRow("modern, theme sheet") << false << true;
        QTest::newRow("bubble, chat window sheet") << true << false;
        QTest::newRow("bubble, theme sheet") << true << true;
    }
    void styleSheetColoursOverridePsiColours()
    {
        // options.ui.chat.css is the chat window's style sheet; a theme's
        // style sheet is set on each row. Either kind of rule must win over
        // the colours Psi computes, whatever makes Psi recompute them.
        QFETCH(bool, bubble);
        QFETCH(bool, themeSheet);
        const QString sheet = QStringLiteral(
            "QLabel#mamAuthor { color: #cc0000; font-style: italic; }"
            "QLabel#mamTimestamp { color: #0000cc; }"
            "QLabel#mamDeliveryIndicator { color: #00aaff; }"
            "QTextEdit#mamBody { color: #008800; padding: 6px; border: 1px solid #999999; }"
            "#mamBubble[tone=\"own\"] { qproperty-fillColor: #dcf8c6; qproperty-textColumns: 25; }"
            "#mamBubble[tone=\"own\"] QTextEdit#mamBody { color: #004400; }");
        QWidget window;
        auto   *layout = new QVBoxLayout(&window);
        auto   *list   = new MamChatWidget(&window);
        layout->addWidget(list);
        if (!themeSheet)
            window.setStyleSheet(sheet);
        list->setHistoryLoading(true);
        list->setTheme({ QStringLiteral("test"), QStringLiteral("Test"), NativeChatTheme::Modern, 40, 6,
                         themeSheet ? sheet : QString() },
                       bubble ? NativeChatRenderers::bubble() : nullptr);
        list->setNicknameColorResolver([](const MessageView &) { return QColor(0x88, 0x44, 0x00); });
        list->appendMessage(message(QStringLiteral("in"), QStringLiteral("Hi")));
        auto own = message(QStringLiteral("own"), QStringLiteral("A longer reply that wraps in a narrow bubble"), 1,
                           true);
        own.setMessageState(MessageState::Delivered);
        list->appendMessage(own);
        window.resize(900, 500);
        window.show();
        QTRY_VERIFY(list->itemWidget(0) && list->itemWidget(1));

        const auto check = [&](const char *step) {
            for (int index : { 0, 1 }) {
                auto *row  = list->itemWidget(index);
                auto *body = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
                const QByteArray where = QByteArray(step) + " row " + QByteArray::number(index);
                QVERIFY2(row->findChild<QLabel *>(QStringLiteral("mamAuthor"))->palette().color(QPalette::WindowText)
                             == QColor(0xcc, 0x00, 0x00),
                         where);
                QVERIFY2(row->findChild<QLabel *>(QStringLiteral("mamAuthor"))->font().italic(), where);
                QVERIFY2(row->findChild<QLabel *>(QStringLiteral("mamTimestamp"))
                                 ->palette()
                                 .color(QPalette::WindowText)
                             == QColor(0x00, 0x00, 0xcc),
                         where);
                const QColor bodyColor = bubble && index == 1 ? QColor(0x00, 0x44, 0x00) : QColor(0x00, 0x88, 0x00);
                QVERIFY2(body->palette().color(QPalette::Text) == bodyColor, where);
                // Border and padding are measured: the whole text is shown.
                QVERIFY2(body->viewport()->height() >= qCeil(body->document()->size().height()), where);
                QVERIFY2(body->contentsMargins().top() >= 7, where);
                // The surface stays Psi's: hover and selection tint it.
                QVERIFY2(body->palette().color(QPalette::Base) == row->palette().color(QPalette::Base), where);
            }
            auto *checks = list->itemWidget(1)->findChild<QLabel *>(QStringLiteral("mamDeliveryIndicator"));
            QVERIFY2(checks && checks->palette().color(QPalette::Text) == QColor(0x00, 0xaa, 0xff), step);
        };
        check("shown");
        list->setNicknameColorResolver([](const MessageView &) { return QColor(0x00, 0x66, 0x66); });
        check("nickname colours");
        QPalette colors = list->palette();
        colors.setColor(QPalette::Base, QColor(0xf0, 0xf0, 0xf0));
        colors.setColor(QPalette::Text, QColor(0x22, 0x22, 0x22));
        list->setPalette(colors);
        QCoreApplication::processEvents();
        QCOMPARE(list->itemWidget(0)->palette().color(QPalette::Base), QColor(0xf0, 0xf0, 0xf0));
        check("palette");
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(list->itemWidget(1), &enter);
        QTRY_VERIFY(list->itemWidget(1)->palette().color(QPalette::Base) != QColor(0xf0, 0xf0, 0xf0));
        check("hover");
        window.setStyleSheet(window.styleSheet()); // As when options.ui.chat.css is edited.
        QCoreApplication::processEvents();
        check("repolish");
        list->setMessageSelected(0, true);
        check("selection");

        if (bubble) {
            auto *own   = list->itemWidget(1)->findChild<QWidget *>(QStringLiteral("mamBubble"));
            auto *other = list->itemWidget(0)->findChild<QWidget *>(QStringLiteral("mamBubble"));
            QCOMPARE(own->property("tone").toString(), QStringLiteral("own"));
            QCOMPARE(other->property("tone").toString(), QStringLiteral("incoming"));
            QCOMPARE(own->property("fillColor").value<QColor>(), QColor(0xdc, 0xf8, 0xc6));
            QVERIFY(!other->property("fillColor").value<QColor>().isValid()); // Follows the palette.
            const QMargins margins = own->layout()->contentsMargins();
            QCOMPARE(own->maximumWidth(),
                     qCeil(QFontMetricsF(own->font()).averageCharWidth() * 25) + margins.left() + margins.right());
            QCOMPARE(own->width(), own->maximumWidth());
            // The row is as tall as the narrower, wrapped bubble.
            QVERIFY(own->mapTo(list->itemWidget(1), QPoint(0, own->height())).y() <= list->itemWidget(1)->height());
        }

        if (!themeSheet) {
            // Without the rules, the parts take Psi's colours again.
            window.setStyleSheet(QString());
            QCoreApplication::processEvents();
            auto *row = list->itemWidget(0);
            QTRY_COMPARE(row->findChild<QLabel *>(QStringLiteral("mamAuthor"))->palette().color(QPalette::WindowText),
                         QColor(0x00, 0x66, 0x66));
            // Qt restores the log's own palette as it was before the sheet;
            // Psi reapplies its chat colours to the log on paletteChanged().
            QCOMPARE(row->findChild<ChatMessageText *>(QStringLiteral("mamBody"))->palette().color(QPalette::Text),
                     list->palette().color(QPalette::Text));
        }
    }

    void themeSetsHoverHighlightAndLogBackground()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Tiles: a red column and a blue column. Wide: red left, blue right.
        QImage tile(2, 2, QImage::Format_RGB32);
        tile.setPixel(0, 0, qRgb(255, 0, 0));
        tile.setPixel(0, 1, qRgb(255, 0, 0));
        tile.setPixel(1, 0, qRgb(0, 0, 255));
        tile.setPixel(1, 1, qRgb(0, 0, 255));
        const QString tilePath = dir.filePath(QStringLiteral("tile.png"));
        QVERIFY(tile.save(tilePath));
        QImage wide(4, 2, QImage::Format_RGB32);
        for (int x = 0; x < 4; ++x)
            for (int y = 0; y < 2; ++y)
                wide.setPixel(x, y, x < 2 ? qRgb(255, 0, 0) : qRgb(0, 0, 255));
        const QString widePath = dir.filePath(QStringLiteral("wide.png"));
        QVERIFY(wide.save(widePath));

        QWidget window;
        auto   *layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *list = new MamChatWidget(&window);
        layout->addWidget(list);
        list->setHistoryLoading(true);
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto pixel = [list](int x, int y) {
            return list->viewport()->grab().toImage().pixelColor(x, y);
        };

        // A theme colour replaces the chat background, for rows too.
        NativeChatTheme theme { QStringLiteral("t"), QStringLiteral("T"), NativeChatTheme::Modern, 40, 10, {}, {} };
        theme.background = QColor(0x20, 0x40, 0x60);
        list->setTheme(theme);
        QCOMPARE(list->viewport()->palette().color(QPalette::Base), theme.background);
        QCOMPARE(pixel(200, 150), theme.background);

        // A scaled image covers the log and keeps its aspect ratio: 4x2
        // becomes 600x300, centred, so red ends in the middle.
        theme.backgroundImage     = widePath;
        theme.backgroundImageMode = NativeChatTheme::Scale;
        list->setTheme(theme);
        QCOMPARE(pixel(20, 150), QColor(255, 0, 0));
        QCOMPARE(pixel(380, 150), QColor(0, 0, 255));
        QCOMPARE(list->property("backgroundImageMode").toString(), QStringLiteral("scale"));

        // Tiled, the pattern repeats from the top left.
        theme.backgroundImage     = tilePath;
        theme.backgroundImageMode = NativeChatTheme::Tile;
        list->setTheme(theme);
        QCOMPARE(pixel(10, 11), QColor(255, 0, 0));
        QCOMPARE(pixel(11, 11), QColor(0, 0, 255));

        // Rows leave the image visible. The row under the pointer is tinted
        // with the text colour, nearly transparent, so the image still shows
        // through; only a selected row paints its surface.
        list->appendMessage(message(QStringLiteral("one"), QStringLiteral("Hello")));
        list->appendMessage(message(QStringLiteral("two"), QStringLiteral("World"), 1));
        QTRY_VERIFY(list->itemWidget(0) && list->itemWidget(1) && list->itemWidget(1)->isVisible());
        auto *row = list->itemWidget(0);
        QVERIFY(!row->autoFillBackground());
        QVERIFY(list->isHoverHighlightEnabled());
        const QPoint margin = row->geometry().topLeft() + QPoint(2, 2); // An even column: red.
        QCOMPARE(pixel(margin.x(), margin.y()), QColor(255, 0, 0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(row, &enter);
        QTRY_VERIFY(pixel(margin.x(), margin.y()) != QColor(255, 0, 0));
        QVERIFY(!row->autoFillBackground());
        const QColor text   = list->viewport()->palette().color(QPalette::Text);
        const QColor tinted = pixel(margin.x(), margin.y());
        QVERIFY2(qAbs(tinted.red() - qRound(255 * 0.88 + text.red() * 0.12)) <= 2
                     && qAbs(tinted.blue() - qRound(text.blue() * 0.12)) <= 2,
                 qPrintable(tinted.name()));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(row, &leave);
        QApplication::sendEvent(list->itemWidget(1), &enter);
        QTRY_COMPARE(pixel(margin.x(), margin.y()), QColor(255, 0, 0));
        list->setMessageSelected(0, true);
        QVERIFY(row->autoFillBackground());
        list->setMessageSelected(0, false);

        // A theme without hover highlight keeps the hover actions.
        theme.hoverHighlight = false;
        list->setTheme(theme);
        QVERIFY(!list->isHoverHighlightEnabled());
        QTRY_VERIFY(list->itemWidget(0));
        row = list->itemWidget(0);
        const QColor surface = row->palette().color(QPalette::Base);
        QApplication::sendEvent(row, &enter);
        auto *actions = list->findChild<QFrame *>(QStringLiteral("mamMessageActions"));
        QTRY_VERIFY(actions->isVisible());
        QCOMPARE(row->palette().color(QPalette::Base), surface);
        QVERIFY(!row->autoFillBackground());

        // The chat window's style sheet overrides the theme.
        window.setStyleSheet(QStringLiteral("MamChatWidget { qproperty-hoverHighlight: true;"
                                            " qproperty-backgroundColor: #102030;"
                                            " qproperty-backgroundImage: \"none\"; }"));
        QCoreApplication::processEvents();
        QVERIFY(list->isHoverHighlightEnabled());
        QCOMPARE(list->viewport()->palette().color(QPalette::Base), QColor(0x10, 0x20, 0x30));
        QVERIFY2(list->property("backgroundImage").toString().isEmpty(), qPrintable(list->property("backgroundImage").toString()));
        list->setTheme(theme); // Still overridden after a theme change.
        QVERIFY(list->isHoverHighlightEnabled());
        QTRY_VERIFY(list->itemWidget(0) && list->itemWidget(1));
        const QPoint corner = list->itemWidget(1)->geometry().topLeft() + QPoint(2, 2);
        QTRY_COMPARE(pixel(corner.x(), corner.y()), QColor(0x10, 0x20, 0x30));
        window.setStyleSheet(QStringLiteral("MamChatWidget { qproperty-backgroundImage: url(%1);"
                                            " qproperty-backgroundImageMode: scale; }")
                                 .arg(widePath));
        QCoreApplication::processEvents();
        QCOMPARE(list->property("backgroundImage").toString(), widePath);
        QCOMPARE(list->property("backgroundImageMode").toString(), QStringLiteral("scale"));
    }

    void oversizedImagesScaleToTheMessageWidth()
    {
        FileSharingDeviceOpener opener;
        FileSharingDeviceOpener account;
        opener.accounts.insert(QStringLiteral("account-a"), &account);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(900, 300, QImage::Format_RGB32);
        image.preview_.fill(QColor(0, 170, 0));
        account.items.insert(QStringLiteral("wide"), &image);

        for (bool bubble : { false, true }) {
            MamChatWidget list;
            list.resize(520, 600);
            list.setHistoryLoading(true);
            list.setMediaOpener(&opener);
            if (bubble)
                list.setTheme({ QStringLiteral("bubble"), QStringLiteral("Bubble"), NativeChatTheme::Modern, 40, 6, {} },
                              NativeChatRenderers::bubble());
            list.appendMessage(
                message(QStringLiteral("wide"), QStringLiteral("<share id=\"wide\" account=\"account-a\"/>"), 0, true));
            list.show();
            QTRY_VERIFY(list.itemWidget(0));
            auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
            QVERIFY(body);
            QTRY_VERIFY(body->width() > 100);
            QVERIFY(body->width() < 900);
            // The whole picture fits the text: no overflow into a view that
            // cannot scroll sideways.
            QTRY_VERIFY(body->document()->size().width() <= body->viewport()->width() + 1);
            QCOMPARE(body->horizontalScrollBar()->value(), 0);
            const int scaled = qRound(300.0 * body->viewport()->width() / 900.0);
            const int height = body->heightForWidth(body->width());
            QVERIFY2(height >= scaled - 1 && height <= scaled + body->fontMetrics().height(),
                     qPrintable(QStringLiteral("height %1, scaled image %2").arg(height).arg(scaled)));
            // And it is painted where it is laid out.
            const QImage shot = body->viewport()->grab().toImage();
            const QPoint centre(qRound(body->viewport()->width() / 2.0 * shot.devicePixelRatio()),
                                qRound(scaled / 2.0 * shot.devicePixelRatio()));
            QCOMPARE(shot.pixelColor(centre), QColor(0, 170, 0));
            // The row is as tall as the scaled picture, not as the picture at
            // its full size: no gap below it.
            auto *row = list.itemWidget(0);
            QTRY_COMPARE(row->height(), row->heightForWidth(row->width()));
            const int bodyBottom = body->mapTo(row, QPoint(0, body->height())).y();
            QVERIFY2(row->height() - bodyBottom <= 16,
                     qPrintable(QStringLiteral("row %1, text ends at %2").arg(row->height()).arg(bodyBottom)));
        }
    }

    void messageTextNeverScrollsItsOwnContent()
    {
        // The sequence seen on Qt 6.10: laid out at width 0 and scrolled to
        // the end, then given its real width together with a height that fits
        // the document. QTextEdit's layout-loop guard then skips adjusting the
        // scroll bars, which used to keep the picture scrolled out of view.
        QWidget window;
        window.resize(700, 600);
        auto  *body = new ChatMessageText(&window);
        QImage picture(1148, 898, QImage::Format_RGB32);
        picture.fill(QColor(0, 170, 0));
        body->setRichText(QStringLiteral("<img src=\"share:wide\" width=\"1148\" height=\"898\"/>"));
        // After the text: setting it clears the document's resources.
        body->document()->addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("share:wide")), picture);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        body->resize(0, 148);
        QCoreApplication::processEvents();
        body->moveCursor(QTextCursor::End);
        body->ensureCursorVisible();
        body->resize(520, body->heightForWidth(520));
        QCoreApplication::processEvents();
        QCOMPARE(body->horizontalScrollBar()->value(), 0);
        QCOMPARE(body->verticalScrollBar()->value(), 0);
        QVERIFY(body->document()->size().width() <= body->viewport()->width() + 1);
        const auto onScreen = [&window, body] {
            const QPoint at = body->viewport()->mapTo(&window, QPoint(body->viewport()->width() / 2, 100));
            const QImage screen = QGuiApplication::primaryScreen()->grabWindow(window.winId()).toImage();
            return screen.pixelColor(QPoint(qRound(at.x() * screen.devicePixelRatio()),
                                            qRound(at.y() * screen.devicePixelRatio())));
        };
        QTRY_COMPARE(onScreen(), QColor(0, 170, 0));
        // Nothing scrolls it later either, e.g. a selection drag past its edge.
        body->horizontalScrollBar()->setValue(body->horizontalScrollBar()->maximum());
        body->verticalScrollBar()->setValue(body->verticalScrollBar()->maximum());
        QCOMPARE(body->horizontalScrollBar()->value(), 0);
        QCOMPARE(body->verticalScrollBar()->value(), 0);
    }

    void scaledImagesAreOnScreenWithoutAResize_data()
    {
        QTest::addColumn<QString>("flow");
        QTest::newRow("cached-before-show") << QStringLiteral("cached-before-show");
        QTest::newRow("cached-after-show") << QStringLiteral("cached-after-show");
        QTest::newRow("downloaded") << QStringLiteral("downloaded");
        QTest::newRow("rebuilt") << QStringLiteral("rebuilt");
    }

    void scaledImagesAreOnScreenWithoutAResize()
    {
        // A picture wider than the bubble must be visible as soon as its row
        // is, not only after a resize relays out the text. Check what is on
        // the screen (the backing store), not a fresh rendering of the widget.
        QFETCH(QString, flow);
        FileSharingDeviceOpener opener;
        FileSharingDeviceOpener account;
        opener.accounts.insert(QStringLiteral("account-a"), &account);
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(640, 480, QImage::Format_RGB32);
        image.preview_.fill(QColor(0, 170, 0));
        image.cached_          = flow != QLatin1String("downloaded");
        image.downloadsItself_ = true;
        account.items.insert(QStringLiteral("picture"), &image);

        MamChatWidget list;
        QFont         small = list.font();
        small.setPointSizeF(8); // Keeps the bubble narrower than the picture.
        list.setFont(small);
        list.resize(900, 700);
        list.setHistoryLoading(true);
        list.setMediaOpener(&opener);
        list.setTheme({ QStringLiteral("bubble"), QStringLiteral("Bubble"), NativeChatTheme::Modern, 40, 6, {} },
                      NativeChatRenderers::bubble());
        for (int i = 0; i < 5; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Earlier message"), i, i % 2));
        const auto picture = message(QStringLiteral("picture"),
                                     QStringLiteral("<share id=\"picture\" account=\"account-a\" auto=\"1048576\"/>"),
                                     100, true);
        if (flow == QLatin1String("cached-before-show"))
            list.appendMessage(picture);
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list));
        if (flow != QLatin1String("cached-before-show"))
            list.appendMessage(picture);
        const int index = list.messageCount() - 1;
        QTRY_VERIFY(list.itemWidget(index) && list.itemWidget(index)->isVisible());
        if (flow == QLatin1String("downloaded")) {
            QTest::qWait(100);
            image.cached_ = true;
            emit image.downloadFinished();
        } else if (flow == QLatin1String("rebuilt")) {
            QTest::qWait(100);
            list.setTheme({ QStringLiteral("bubble"), QStringLiteral("Bubble"), NativeChatTheme::Modern, 40, 6, {} },
                          NativeChatRenderers::bubble());
        }
        const auto onScreen = [&list, index]() -> QColor {
            auto *row  = list.itemWidget(index);
            auto *body = row ? row->findChild<ChatMessageText *>() : nullptr;
            if (!body || !body->isVisible() || body->viewport()->height() < 20)
                return {};
            const QPoint at = body->viewport()->mapTo(
                list.window(), QPoint(body->viewport()->width() / 2, qMin(body->viewport()->height() / 2, 100)));
            const QImage screen = QGuiApplication::primaryScreen()->grabWindow(list.window()->winId()).toImage();
            const QPoint pixel(qRound(at.x() * screen.devicePixelRatio()), qRound(at.y() * screen.devicePixelRatio()));
            return screen.rect().contains(pixel) ? screen.pixelColor(pixel) : QColor();
        };
        const auto state = [&list, index]() {
            auto *body = list.itemWidget(index)->findChild<ChatMessageText *>();
            return QStringLiteral("viewport %1x%2, text width %3, document %4x%5, scroll %6/%7 %8/%9")
                .arg(body->viewport()->width())
                .arg(body->viewport()->height())
                .arg(body->document()->textWidth())
                .arg(body->document()->size().width())
                .arg(body->document()->size().height())
                .arg(body->horizontalScrollBar()->value())
                .arg(body->horizontalScrollBar()->maximum())
                .arg(body->verticalScrollBar()->value())
                .arg(body->verticalScrollBar()->maximum());
        };
        QTRY_VERIFY2(onScreen() == QColor(0, 170, 0), qPrintable(state()));
        auto *body = list.itemWidget(index)->findChild<ChatMessageText *>();
        QVERIFY2(body->document()->size().width() <= body->viewport()->width() + 1, qPrintable(state()));
        QVERIFY2(qAbs(body->document()->textWidth() - body->viewport()->width()) <= 1, qPrintable(state()));
    }

    void nativeModernClassicAndPluginPlacement()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(620, 360);
        list.appendMessage(message(QStringLiteral("layout"), QStringLiteral("Message body")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        auto *modern = list.itemWidget(0);
        auto *modernNick = modern->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        auto *modernBody = modern->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QCOMPARE(modernNick->text(), QStringLiteral("Peer"));
        QVERIFY(modernBody->mapTo(modern, QPoint()).y() > modernNick->mapTo(modern, QPoint()).y());

        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        // Virtual rows stage replacement until Qt's rich-text height settles.
        // Classic is the legacy text log: icon, time and nickname in the text.
        QTRY_VERIFY(list.itemWidget(0) && !list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor")));
        auto *row = list.itemWidget(0);
        QVERIFY(!row->findChild<QLabel *>(QStringLiteral("mamTimestamp")));
        auto *body = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        const QString hhmmss = QDateTime::fromString(QStringLiteral("2026-10-18T10:00:00Z"),
                                                      Qt::ISODate).toLocalTime().toString(QStringLiteral("HH:mm:ss"));
        QCOMPARE(body->toPlainText(), QString(QChar::ObjectReplacementCharacter) + QStringLiteral(" [") + hhmmss
                     + QStringLiteral("] <Peer> Message body"));

        NativeChatTheme plugin;
        plugin.layout = NativeChatTheme::Modern;
        plugin.avatarSize = 0;
        plugin.layoutSpec = QStringLiteral(R"({"direction":"vertical","items":["body",{"direction":"horizontal","items":["timestamp","author"]}]})");
        list.setTheme(plugin);
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor")));
        QTRY_VERIFY(list.itemWidget(0)->findChild<ChatMessageText *>(QStringLiteral("mamBody"))
                        ->mapTo(list.itemWidget(0), QPoint()).y()
                    <= list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"))
                        ->mapTo(list.itemWidget(0), QPoint()).y());
        row        = list.itemWidget(0);
        auto *nick = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        body       = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QVERIFY(body->mapTo(row, QPoint()).y() <= nick->mapTo(row, QPoint()).y());
        QVERIFY(time->mapTo(row, QPoint()).x() < nick->mapTo(row, QPoint()).x());
        list.setNicknameColorResolver([](const MessageView &) { return QColor(Qt::red); });
        QCOMPARE(nick->palette().color(QPalette::Text), QColor(Qt::red));
        list.setTheme(classic);
        QTRY_VERIFY(list.itemWidget(0) && !list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor")));
        body = list.itemWidget(0)->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QCOMPARE(body->document()->find(QStringLiteral("<Peer>")).charFormat().foreground().color(), QColor(Qt::red));
    }

    void classicTextLogFlowsLikeTheLegacyLog()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        list.setHistoryLoading(true);
        list.resize(420, 600);
        NativeChatTheme classic;
        classic.layout     = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        list.setNicknameColorResolver([](const MessageView &) { return QColor(60, 95, 150); });
        auto incoming = message(QStringLiteral("in"), QStringLiteral("A long message ").repeated(12));
        incoming.setNick(QStringLiteral("a rather long nickname"));
        list.appendMessage(incoming);
        auto own = message(QStringLiteral("own"), QStringLiteral("Mine"), 1, true);
        own.setEncrypted(true);
        list.appendMessage(own);
        auto spooled = message(QStringLiteral("old"), QStringLiteral("From history"), 2);
        spooled.setSpooled(true);
        list.appendMessage(spooled);
        MessageView status(MessageView::Status);
        status.setPlainText(QStringLiteral("Peer is away"));
        list.appendMessage(status);
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2) && list.itemWidget(3));
        const auto bodyOf = [&list](int index) {
            return list.itemWidget(index)->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        };
        const auto icons = [&](int index) {
            QStringList names;
            for (auto block = bodyOf(index)->document()->begin(); block.isValid(); block = block.next())
                for (auto it = block.begin(); !it.atEnd(); ++it)
                    if (it.fragment().charFormat().isImageFormat())
                        names << it.fragment().charFormat().toImageFormat().name();
            return names;
        };
        // The message icon before the time, as in the legacy chat view.
        QCOMPARE(icons(0), QStringList { QStringLiteral("psi-icon:psi/notification_chat_receive") });
        QCOMPARE(icons(1), QStringList { QStringLiteral("psi-icon:psi/notification_chat_delivery_ok_encrypted") });
        QCOMPARE(icons(2), QStringList { QStringLiteral("psi-icon:psi/history") });
        QCOMPARE(icons(3), QStringList { QStringLiteral("psi-icon:psi/notification_chat_info") });
        // No lock or check marks beside it.
        QVERIFY(!list.itemWidget(1)->findChild<QLabel *>(QStringLiteral("mamEncryptionIndicator")));

        // Wrapped lines start at the left edge, not under the nickname's end.
        auto *body = bodyOf(0);
        QTRY_VERIFY(body->document()->begin().layout()->lineCount() > 2);
        QTextLayout *layout = body->document()->begin().layout();
        QCOMPARE(layout->lineAt(1).x(), layout->lineAt(0).x());
        // The nickname is in its colour and inserts it when clicked.
        const QTextCursor nick = body->document()->find(QStringLiteral("<a rather long nickname>"));
        QCOMPARE(nick.charFormat().foreground().color(), QColor(60, 95, 150));
        QSignalSpy clicked(&list, &MamChatWidget::nicknameClicked);
        QTextCursor middle(nick);
        middle.setPosition(nick.selectionStart() + 3);
        const QPoint point = body->cursorRect(middle).center();
        QTest::mouseClick(body->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(clicked.size(), 1);
        QCOMPARE(clicked.first().first().toString(), QStringLiteral("a rather long nickname"));
        // Nickname colours change in place.
        auto *row = list.itemWidget(0);
        list.setNicknameColorResolver([](const MessageView &) { return QColor(150, 60, 60); });
        QCOMPARE(list.itemWidget(0), row);
        QCOMPARE(body->document()->find(QStringLiteral("<a rather long nickname>")).charFormat().foreground().color(),
                 QColor(150, 60, 60));
        QCOMPARE(body->document()->find(QStringLiteral("[")).charFormat().foreground().color(), QColor(150, 60, 60));
        // Without message icons, as with the legacy option off.
        list.setMessageIconsEnabled(false);
        QTRY_VERIFY(list.itemWidget(1) && icons(1).isEmpty());
    }

    void conversationHeadersUseSentAndReceivedColours()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(420, 400);
        NativeChatTheme classic;
        classic.layout     = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        list.setDirectionColors(QColor("#ff0000"), QColor("#0000ff"));
        list.appendMessage(message(QStringLiteral("in"), QStringLiteral("Hello")));
        list.appendMessage(message(QStringLiteral("out"), QStringLiteral("Hi"), 1, true));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));
        // As in the legacy text log: time and nickname in the colour of
        // received or sent messages, the text in the text colour.
        const auto colourOf = [&list](int index, const QString &text) {
            return list.itemWidget(index)
                ->findChild<ChatMessageText *>(QStringLiteral("mamBody"))
                ->document()
                ->find(text)
                .charFormat()
                .foreground()
                .color();
        };
        QCOMPARE(colourOf(0, QStringLiteral("<Peer>")), QColor("#0000ff"));
        QCOMPARE(colourOf(0, QStringLiteral("[")), QColor("#0000ff"));
        QCOMPARE(colourOf(1, QStringLiteral("[")), QColor("#ff0000"));
        QVERIFY(colourOf(0, QStringLiteral("Hello")) != QColor("#0000ff"));
        // The colours follow the options, in place.
        auto *row = list.itemWidget(0);
        list.setDirectionColors(QColor("#aa0000"), QColor("#0000aa"));
        QCOMPARE(list.itemWidget(0), row);
        QCOMPARE(colourOf(0, QStringLiteral("<Peer>")), QColor("#0000aa"));

        // Modern colours the nickname the same way; Bubble, whose bubbles
        // already tell sent from received, does not.
        list.setTheme({ QStringLiteral("modern"), QStringLiteral("Modern"), NativeChatTheme::Modern, 40, 10, {} });
        const auto author = [&list](int index) {
            auto *row = list.itemWidget(index);
            return row ? row->findChild<QLabel *>(QStringLiteral("mamAuthor")) : nullptr;
        };
        QTRY_VERIFY(author(0) && author(1));
        QCOMPARE(author(0)->palette().color(QPalette::WindowText), QColor("#0000aa"));
        QCOMPARE(author(1)->palette().color(QPalette::WindowText), QColor("#aa0000"));
        list.setTheme({ QStringLiteral("bubble"), QStringLiteral("Bubble"), NativeChatTheme::Modern, 40, 6, {} },
                      NativeChatRenderers::bubble());
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(0)->findChild<QWidget *>(QStringLiteral("mamBubble"))
                    && author(0));
        QCOMPARE(author(0)->palette().color(QPalette::WindowText), list.palette().color(QPalette::Text));
    }

    void localIdentityMergesWithoutClientStanzaId()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,500);
        auto local=message({},QStringLiteral("Original local body"));
        local.setHistoryId(42);
        list.appendMessage(local);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        list.setMessageSelected(0,true);
        auto archived=local;
        archived.setArchiveId(QStringLiteral("server-uid"));
        list.prependArchivePage({qMakePair(archived.archiveId(),archived)});
        QCOMPARE(list.messageCount(),1);
        QCOMPARE(list.selectedMessageCount(),1);
        // A source UID aliases the original EDB row, preserving semantic
        // selection anchors instead of replacing them on archive adoption.
        QCOMPARE(list.itemWidget(0)->property("_psiMamRowKey").toString(),QStringLiteral("edb:42"));
        list.appendMessage(archived);
        QCOMPARE(list.messageCount(),1);
    }
    void localOnlyRowsStayInChronologicalPosition()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        auto local=message({},QStringLiteral("Only in local history"),20);
        local.setHistoryId(42);
        list.appendMessage(local);
        auto old=message(QStringLiteral("old"),QStringLiteral("Old server message"),10);
        auto recent=message(QStringLiteral("recent"),QStringLiteral("Recent server message"),30);
        list.prependArchivePage({qMakePair(QStringLiteral("a"),old),qMakePair(QStringLiteral("b"),recent)});
        QCOMPARE(list.messageCount(),3);
        list.show();
        QTRY_VERIFY(list.itemWidget(2));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Old server message"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Only in local history"));
        QCOMPARE(list.itemWidget(2)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Recent server message"));
    }
    void equalTimestampsKeepServerPageOrder()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        auto old=message(QStringLiteral("old"),QStringLiteral("First"));
        auto recent=message(QStringLiteral("new"),QStringLiteral("Second"));
        list.prependArchivePage({qMakePair(QStringLiteral("z"),old),qMakePair(QStringLiteral("a"),recent)});
        list.show();
        QTRY_VERIFY(list.itemWidget(1));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("First"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Second"));
    }
    void overlappingEqualTimestampPagesUseArchiveOrder()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        const auto first=message(QStringLiteral("first"),QStringLiteral("First"));
        const auto second=message(QStringLiteral("second"),QStringLiteral("Second"));
        const auto third=message(QStringLiteral("third"),QStringLiteral("Third"));
        list.prependArchivePage({qMakePair(QStringLiteral("first"),first),qMakePair(QStringLiteral("second"),second)});
        list.prependArchivePage({qMakePair(QStringLiteral("second"),second),qMakePair(QStringLiteral("third"),third)});
        list.show();
        QCOMPARE(list.messageCount(),3);
        QTRY_VERIFY(list.itemWidget(2));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("First"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Second"));
        QCOMPARE(list.itemWidget(2)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Third"));
    }
    void chatFontUpdatesExistingAndNewDocuments()
    {
        MamChatWidget list;
        QFont small = list.font();
        small.setPointSize(9);
        list.setFont(small);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("old"), QStringLiteral("<b>Bold</b> plain text")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *original = list.itemWidget(0);
        auto *body = original->findChild<ChatMessageText *>();
        auto cursor = body->document()->find(QStringLiteral("Bold"));
        QCOMPARE(cursor.charFormat().fontPointSize(), 9.0);
        const auto weight = cursor.charFormat().fontWeight();
        QFont large = small;
        large.setPointSize(18);
        list.setFont(large);
        list.appendMessage(message(QStringLiteral("new"), QStringLiteral("New message")));
        QTRY_VERIFY(list.itemWidget(1));
        QCOMPARE(list.itemWidget(0), original); // Formatting changes do not replace the row/model.
        for (int i = 0; i < 2; ++i) {
            auto *row = list.itemWidget(i);
            QCOMPARE(row->findChild<QLabel *>(QStringLiteral("mamAuthor"))->font().pointSize(), 18);
            auto *text = row->findChild<ChatMessageText *>();
            for (auto block = text->document()->begin(); block.isValid(); block = block.next())
                for (auto it = block.begin(); !it.atEnd(); ++it)
                    QCOMPARE(it.fragment().charFormat().fontPointSize(), 18.0);
        }
        QCOMPARE(body->document()->find(QStringLiteral("Bold")).charFormat().fontWeight(), weight);
    }

    void groupNicknameClickUsesPlainNickname()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        auto view = message(QStringLiteral("nick"), QStringLiteral("Message"));
        view.setNick(QStringLiteral("Alice <&> /"));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *author = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QSignalSpy clicked(&list, &MamChatWidget::nicknameClicked);
        QTest::mouseClick(author, Qt::LeftButton);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().first().toString(), view.nick());
        QTest::mouseClick(author, Qt::LeftButton, Qt::ControlModifier);
        QCOMPARE(list.selectedMessageCount(), 1);
        QCOMPARE(clicked.count(), 1);
        list.cancelMessageSelection();
        list.setGroupChat(false);
        QTest::mouseClick(author, Qt::LeftButton);
        QCOMPARE(clicked.count(), 1);
    }

    void paletteUpdatesExistingDocumentLinks()
    {
        ChatMessageText body;
        QPalette light = body.palette();
        light.setColor(QPalette::Link, QColor(0, 50, 120));
        body.setPalette(light);
        body.setRichText(QStringLiteral("<a href=\"https://example.org\" style=\"color:#000080\">Link</a>"));
        auto link = body.document()->find(QStringLiteral("Link"));
        QCOMPARE(link.charFormat().foreground().color(), light.color(QPalette::Link));
        QPalette dark = light;
        dark.setColor(QPalette::Link, QColor(128, 184, 255));
        body.setPalette(dark);
        link = body.document()->find(QStringLiteral("Link"));
        QCOMPARE(link.charFormat().foreground().color(), dark.color(QPalette::Link));
        QCOMPARE(link.charFormat().anchorHref(), QStringLiteral("https://example.org"));
    }

    void desktopPaletteStorageAndPreview()
    {
        QPalette light;
        light.setColor(QPalette::Base, Qt::white);
        light.setColor(QPalette::Text, Qt::black);
        const auto dark = DesktopColorPalette::preview(QStringLiteral("dark"), light);
        QCOMPARE(DesktopColorPalette::name(light), QStringLiteral("light"));
        QCOMPARE(DesktopColorPalette::name(dark), QStringLiteral("dark"));
        const QString logical = QStringLiteral("options.ui.look.colors.chat.background");
        QCOMPARE(DesktopColorPalette::optionPath(logical, QStringLiteral("dark")),
                 QStringLiteral("options.ui.look.palettes.dark.chat.background"));
        QVERIFY(DesktopColorPalette::optionPath(logical, QStringLiteral("light"))
            != DesktopColorPalette::optionPath(logical, QStringLiteral("dark")));
        QCOMPARE(DesktopColorPalette::preview(QStringLiteral("dark"), dark), dark);
        QCOMPARE(DesktopColorPalette::preview(QStringLiteral("light"), dark).color(QPalette::Base), QColor(Qt::white));
        MamChatWidget list;
        list.setHoverHighlightEnabled(false); // Assert the unhovered palette regardless of cursor position.
        list.resize(550, 450);
        list.setPalette(light);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("one"), QStringLiteral("Message")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *row = list.itemWidget(0);
        list.setPalette(dark);
        QCOMPARE(list.itemWidget(0), row);
        QCOMPARE(row->palette().color(QPalette::Base), dark.color(QPalette::Base));
        QCOMPARE(row->findChild<ChatMessageText *>()->palette().color(QPalette::Text), dark.color(QPalette::Text));
        list.setPalette(light);
        QCOMPARE(row->palette().color(QPalette::Base), QColor(Qt::white));
    }

    void applicationPaletteUpdatesExistingRows_data()
    {
        QTest::addColumn<bool>("classic");
        QTest::addColumn<bool>("groupChat");
        QTest::newRow("modern-chat") << false << false;
        QTest::newRow("classic-chat") << true << false;
        QTest::newRow("modern-muc") << false << true;
        QTest::newRow("classic-muc") << true << true;
    }

    void applicationPaletteUpdatesExistingRows()
    {
        QFETCH(bool, classic);
        QFETCH(bool, groupChat);
        const QPalette original = QApplication::palette();
        const auto     restore  = qScopeGuard([&] { QApplication::setPalette(original); });
        QPalette       light    = DesktopColorPalette::preview(QStringLiteral("light"),
                                                               DesktopColorPalette::preview(QStringLiteral("dark"), original));
        const QPalette dark     = DesktopColorPalette::preview(QStringLiteral("dark"), light);
        QApplication::setPalette(light);
        QWidget window;
        // Chat windows have a stylesheet, even when it only styles correction
        // mode in the composer. Qt then changes palette propagation for all
        // of the window's children, including unrelated message widgets.
        window.setStyleSheet(QStringLiteral("QTextEdit[correction=\"true\"] { background-color: rgb(160,160,0); }"));
        window.resize(550, 450);
        MamChatWidget list(&window);
        list.setHoverHighlightEnabled(false); // Hover/palette interaction is covered separately.
        list.setGroupChat(groupChat);
        const QColor nickColor(0, 170, 170);
        if (groupChat)
            list.setNicknameColorResolver([nickColor](const MessageView &) { return nickColor; });
        if (classic)
            list.setTheme({ "classic", "Classic", NativeChatTheme::Classic, 0, 4, {} });
        list.resize(550, 450);
        list.setHistoryLoading(true);
        auto apply = [&] {
            QPalette palette = QApplication::palette();
            palette.setColor(QPalette::Base, palette.color(QPalette::Base));
            palette.setColor(QPalette::Window, palette.color(QPalette::Base));
            palette.setColor(QPalette::Text, palette.color(QPalette::Text));
            const QSignalBlocker blocker(&list);
            list.setPalette(palette);
            list.viewport()->setPalette(list.palette());
            list.setInformationalColor(palette.color(QPalette::Base).lightness() < 128 ? QColor("#8bd58b")
                                                                                       : QColor("#008000"));
        };
        connect(&list, &MamChatWidget::paletteChanged, &list, apply);
        apply();
        list.appendMessage(message("existing",
                                   "<b>Existing text</b> <a href='https://example.org'>link</a> "
                                   "<span style='color:#ff0000'>red</span>"));
        MessageView subject(MessageView::Subject);
        subject.setPlainText("Room subject");
        list.appendMessage(subject);
        window.show();
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));
        auto *row      = list.itemWidget(0);
        auto *body     = row->findChild<ChatMessageText *>();
        auto *document = body->document();
        body->selectAll();
        for (const auto &palette : { dark, light }) {
            QApplication::setPalette(palette);
            apply();
            window.setStyleSheet(window.styleSheet()); // KDE/Psi repolishes after the palette update.
            QCoreApplication::processEvents();
            QCOMPARE(list.itemWidget(0), row);
            QCOMPARE(row->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(body->palette().color(QPalette::Text), palette.color(QPalette::Text));
            QCOMPARE(body->viewport()->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(body->viewport()->palette().color(QPalette::Text), palette.color(QPalette::Text));
            QColor timeColor = classic && groupChat ? nickColor : palette.color(QPalette::Text);
            if (!classic)
                timeColor.setAlpha(188);
            if (classic) {
                // The legacy text log has the time and nickname in the text.
                QCOMPARE(document->find(QStringLiteral("<Peer>")).charFormat().foreground().color(),
                         groupChat ? nickColor : palette.color(QPalette::Text));
                QCOMPARE(document->find(QStringLiteral("[")).charFormat().foreground().color(), timeColor);
            } else {
                QCOMPARE(row->findChild<QLabel *>("mamAuthor")->palette().color(QPalette::Text),
                         groupChat ? nickColor : palette.color(QPalette::Text));
                QCOMPARE(row->findChild<QLabel *>("mamTimestamp")->palette().color(QPalette::Text), timeColor);
            }
            auto *subjectBody = list.itemWidget(1)->findChild<ChatMessageText *>();
            QCOMPARE(subjectBody->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(subjectBody->palette().color(QPalette::Text),
                     palette.color(QPalette::Base).lightness() < 128 ? QColor("#8bd58b") : QColor("#008000"));
            QCOMPARE(body->document(), document);
            QVERIFY(body->textCursor().selectedText().endsWith(QString("Existing text link red")));
            QCOMPARE(document->find("link").charFormat().foreground().color(), palette.color(QPalette::Link));
            QCOMPARE(document->find("red").charFormat().foreground().color(), QColor(Qt::red));
            QCOMPARE(document->find("Existing").charFormat().fontWeight(), int(QFont::Bold));
        }
    }

    void wholeMessageSelectionColorsAndText_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void wholeMessageSelectionColorsAndText()
    {
        QFETCH(bool, dark);
        MamChatWidget list;
        list.setHoverHighlightEnabled(false); // Cancellation should restore the unhovered surface here.
        QPalette palette = list.palette();
        const QColor base = dark ? QColor(25, 25, 25) : QColor(Qt::white);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Window, base);
        palette.setColor(QPalette::Text, dark ? Qt::white : Qt::black);
        palette.setColor(QPalette::Highlight, QColor(0, 120, 230));
        list.setPalette(palette);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Selectable message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        auto *first = list.itemWidget(0)->findChild<ChatMessageText *>();
        auto *other = list.itemWidget(2)->findChild<ChatMessageText *>();
        first->selectAll();
        other->selectAll();
        QVERIFY(first->textCursor().hasSelection() && other->textCursor().hasSelection());
        list.selectMessageRange(0, 1);
        QVERIFY(!first->textCursor().hasSelection());
        QVERIFY(!other->textCursor().hasSelection());
        QCOMPARE(first->textInteractionFlags(), Qt::TextInteractionFlags(Qt::NoTextInteraction));
        const QColor selected = list.itemWidget(0)->palette().color(QPalette::Base);
        QVERIFY(selected != base);
        auto bodyPixel = [&list](int index) {
            auto *body = list.itemWidget(index)->findChild<ChatMessageText *>();
            const auto point = body->viewport()->mapTo(&list, QPoint(body->viewport()->width() - 3,
                                                                     body->viewport()->height() - 3));
            const auto image = list.grab().toImage();
            return image.pixelColor(qRound(point.x() * image.devicePixelRatio()),
                                    qRound(point.y() * image.devicePixelRatio()));
        };
        QTRY_COMPARE(bodyPixel(0), selected);
        QTRY_COMPARE(bodyPixel(1), selected);
        QTRY_COMPARE(bodyPixel(2), base);
        list.cancelMessageSelection();
        QTRY_COMPARE(bodyPixel(0), base);
        QVERIFY(first->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
        first->selectAll();
        QVERIFY(first->textCursor().hasSelection());
    }

    void draggingFromTextClearsThePartialSelection()
    {
        MamChatWidget list;
        list.resize(550, 450);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Selectable message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(2));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        auto *target = body->viewport();
        const QPointF start(2, 7);
        QMouseEvent press(QEvent::MouseButtonPress, start,
                          target->mapTo(target->window(), (start).toPoint()), target->mapToGlobal((start).toPoint()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QMouseEvent inside(QEvent::MouseMove, QPointF(60, 7),
                          target->mapTo(target->window(), (QPointF(60, 7)).toPoint()), target->mapToGlobal((QPointF(60, 7)).toPoint()),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &inside);
        QVERIFY(body->textCursor().hasSelection());
        const auto third = target->mapFromGlobal(list.itemWidget(2)->mapToGlobal(QPoint(60, 20)));
        QMouseEvent outside(QEvent::MouseMove, QPointF(third),
                          target->mapTo(target->window(), (QPointF(third)).toPoint()), target->mapToGlobal((QPointF(third)).toPoint()),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &outside);
        QCOMPARE(list.selectedMessageCount(), 3);
        QVERIFY(!body->textCursor().hasSelection());
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(third),
                          target->mapTo(target->window(), (QPointF(third)).toPoint()), target->mapToGlobal((QPointF(third)).toPoint()),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(target, &release);
        QVERIFY(!body->textCursor().hasSelection());
        list.cancelMessageSelection();
        QApplication::sendEvent(target, &press);
        QApplication::sendEvent(target, &inside);
        QVERIFY(body->textCursor().hasSelection());
    }

    void informationalRowsHaveSemanticPresentation()
    {
        for (bool classic : {false, true}) {
            MamChatWidget list;
            if (classic)
                list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
            list.setInformationalColor(QColor(0, 128, 0));
            list.resize(650, 550);
            list.setHistoryLoading(true);
            const auto ordinary = message(QStringLiteral("ordinary"), QStringLiteral("Ordinary message"));
            list.appendMessage(ordinary);
            const QList<MessageView::Type> types {MessageView::System, MessageView::Subject, MessageView::Status,
                MessageView::MUCJoin, MessageView::MUCPart, MessageView::NickChange, MessageView::Urls,
                MessageView::FileTransferRequest, MessageView::FileTransferFinished};
            for (auto type : types) {
                MessageView view(type);
                view.setPlainText(type == MessageView::Subject ? QStringLiteral("Room subject") : QStringLiteral("System notice"));
                view.setDateTime(ordinary.dateTime());
                list.appendMessage(view);
            }
            list.show();
            QTRY_VERIFY(list.itemWidget(types.size()));
            if (!classic) {
                auto *stamp = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
                QVERIFY(stamp);
                QCOMPARE(stamp->toolTip(), MamChatWidget::fullTimestamp(ordinary.dateTime()));
            }
            for (int i = 1; i <= types.size(); ++i) {
                auto *row = list.itemWidget(i);
                auto *body = row->findChild<ChatMessageText *>();
                QVERIFY(body);
                QVERIFY(!row->findChild<QLabel *>(QStringLiteral("mamTimestamp")));
                QVERIFY(!row->findChild<QLabel *>(QStringLiteral("mamAuthor")));
                QCOMPARE(body->palette().color(QPalette::Text), QColor(0, 128, 0));
                QVERIFY(body->viewport()->toolTip().isEmpty());
                QVERIFY(row->toolTip().isEmpty());
                QCOMPARE(body->document()->begin().blockFormat().alignment() & Qt::AlignHorizontal_Mask,
                         Qt::Alignment(types.at(i - 1) == MessageView::Subject || classic ? Qt::AlignLeft
                                                                                          : Qt::AlignHCenter));
            }
            if (!classic && !qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_SYSTEM_SCREENSHOT"))
                list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_SYSTEM_SCREENSHOT"));
            list.setInformationalColor(QColor(100, 0, 140));
            QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText *>()->palette().color(QPalette::Text), QColor(100, 0, 140));
        }
    }

    void messageSelectionChecks_data()
    {
        QTest::addColumn<bool>("classic");
        QTest::newRow("modern") << false;
        QTest::newRow("classic") << true;
    }
    void messageSelectionChecks()
    {
        QFETCH(bool, classic);
        MamChatWidget list;
        if (classic)
            list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        list.resize(550, 480);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        auto *first = list.itemWidget(0)->findChild<QCheckBox *>();
        auto *second = list.itemWidget(1)->findChild<QCheckBox *>();
        auto *third = list.itemWidget(2)->findChild<QCheckBox *>();
        QVERIFY(first && second && third);
        QVERIFY(first->isHidden() && second->isHidden() && third->isHidden());
        list.selectMessageRange(0, 1);
        QCOMPARE(list.selectedMessageCount(), 2);
        QVERIFY(first->isVisible() && first->isChecked());
        QVERIFY(second->isVisible() && second->isChecked());
        QVERIFY(third->isVisible() && !third->isChecked());
        QTest::mouseClick(third, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 3);
        QVERIFY(third->isChecked());
        QTest::mouseClick(first, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 2);
        QVERIFY(!first->isChecked());
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(first->isHidden() && second->isHidden() && third->isHidden());
        list.selectMessageRange(0, 2);
        list.clearMessages(); // Cancel while rows still have valid model entries.
        QCOMPARE(list.selectedMessageCount(), 0);
        QCOMPARE(list.messageCount(), 0);
    }

    void selectionCheckUsesCurrentRowIdentity()
    {
        MamChatWidget list;
        list.resize(550, 480);
        list.setHistoryLoading(true);
        const auto live = message(QStringLiteral("live"), QStringLiteral("Live message"));
        list.appendMessage(live);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        list.setMessageSelected(0, true);
        // Adopt an archive UID without replacing the visible row.
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("adopted"), live)}), 0);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("older"),
            message(QStringLiteral("old"), QStringLiteral("Older message"), -10))}), 1);
        QTRY_VERIFY(list.itemWidget(1));
        auto *check = list.itemWidget(1)->findChild<QCheckBox *>();
        QVERIFY(check && check->isChecked());
        QTest::mouseClick(check, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(check->isHidden());
    }

    void rowsUseDocumentHeightAtActualWidth_data()
    {
        QTest::addColumn<bool>("modern");
        QTest::newRow("modern") << true;
        QTest::newRow("classic") << false;
    }
    void rowsUseDocumentHeightAtActualWidth()
    {
        QFETCH(bool, modern);
        MamChatWidget list;
        if (!modern)
            list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        list.resize(750, 700);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("short"), QStringLiteral("Short message")));
        list.appendMessage(message(QStringLiteral("long"), QStringLiteral("Wrapping message text with several words. ").repeated(12)));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));
        auto *row = list.itemWidget(0);
        auto *body = row->findChild<ChatMessageText *>();
        auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        // Classic has the nickname in the text.
        QVERIFY(body && (author || !modern));
        if (modern) {
            const int authorBottom = author->mapTo(row, QPoint(0, author->height())).y();
            const int bodyTop      = body->mapTo(row, QPoint()).y();
            QVERIFY2(bodyTop - authorBottom <= 6, "Header/body spacing must not absorb excess row height");
        }
        const int authorHeight = author ? author->height() : 0;
        // Rich QTextDocument height-for-width settles after materialization.
        // Also count the avatar: on Qt6 a 40px avatar can be taller than the
        // combined author/body text, and determines the minimum row height.
        auto *avatar = row->findChild<QLabel *>(QStringLiteral("mamAvatar"));
        const int avatarHeight = avatar ? avatar->height() : 0;
        QTRY_VERIFY2(row->height() <= qMax(avatarHeight,
                                           authorHeight + body->heightForWidth(body->width()) + 3)
                                            + 18,
                     qPrintable(QStringLiteral("row=%1 avatar=%2 author=%3 body=%4 width=%5")
                                    .arg(row->height()).arg(avatarHeight).arg(authorHeight)
                                    .arg(body->heightForWidth(body->width())).arg(body->width())));
        auto *wrapped = list.itemWidget(1)->findChild<ChatMessageText *>();
        QVERIFY(wrapped);
        QTRY_VERIFY(qAbs(wrapped->height() - wrapped->heightForWidth(wrapped->width())) <= 1);
        const int wideHeight = list.itemWidget(1)->height();
        list.resize(340, 700);
        QTRY_VERIFY(list.itemWidget(1)->height() > wideHeight);
        list.resize(750, 700);
        QTRY_COMPARE(list.itemWidget(1)->height(), wideHeight);
        if (modern && !qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_GEOMETRY_SCREENSHOT"))
            list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_GEOMETRY_SCREENSHOT"));
    }

    void psiIconsRenderAndCopyAsEmoticons()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        const QString rich = QStringLiteral("Hello <icon name=\"test/smile\" text=\":)\"> world");
        list.appendMessage(message(QStringLiteral("icon"), rich));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QVERIFY(body->document()->documentLayout()->handlerForObject(QTextFormat::UserObject));
        QVERIFY(body->toPlainText().contains(QChar::ObjectReplacementCharacter));
        const auto pixels = body->viewport()->grab().toImage();
        int iconPixels = 0;
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                if (pixels.pixelColor(x, y) == QColor(245, 10, 220))
                    ++iconPixels;
        QVERIFY2(iconPixels >= 100, "PsiRichText's actual object handler must paint the icon asset");
        body->selectAll();
        body->copy();
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Hello :) world"));
        QTest::mouseClick(body->viewport(), Qt::LeftButton, Qt::ControlModifier, QPoint(2, 2));
        QCOMPARE(list.selectedMessageCount(), 1);
        QVERIFY(!body->textCursor().hasSelection());
        list.copySelectedMessages();
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("Hello :) world")));
    }

    void wheelOverTextScrollsTimeline()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        for (int i = 0; i < 30; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Message text"), i));
        list.show();
        list.scrollToIndex(0, MamChatWidget::Alignment::Top);
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        QTRY_COMPARE(list.verticalScrollBar()->value(), 0);
        QTRY_VERIFY(list.itemWidget(0));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QWheelEvent wheel(QPointF(4, 4), QPointF(body->viewport()->mapToGlobal(QPoint(4, 4))),
                          QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(body->viewport(), &wheel);
        QTRY_VERIFY(list.verticalScrollBar()->value() > 0);
        QCOMPARE(body->verticalScrollBar()->value(), 0);
    }

    void semanticTimestampBoundaries()
    {
        const auto now = QDateTime::fromString(QStringLiteral("2026-10-18T12:34:56+03:00"), Qt::ISODate);
        QCOMPARE(MamChatWidget::displayTime({}, now), QString());
        QCOMPARE(MamChatWidget::displayTime(now, now), QStringLiteral("just now"));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(-59), now), QStringLiteral("just now"));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-60), now).contains(QStringLiteral("1")));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-3600), now).contains(QStringLiteral("1")));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-86399), now).contains(QStringLiteral("23")));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(-86400), now),
                 now.addSecs(-86400).toLocalTime().toString(QStringLiteral("HH:mm")));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(60), now),
                 now.addSecs(60).toLocalTime().toString(QStringLiteral("HH:mm")));
        QVERIFY(MamChatWidget::fullTimestamp(now).contains(now.toLocalTime().toString(QStringLiteral("HH:mm:ss"))));
    }

    void timestampUpdatesOnlyWhileVisibleAndOnlyAtBoundaries()
    {
        MamChatWidget list;
        auto *clock = list.findChild<QTimer *>(QStringLiteral("mamTimestampRefresh"));
        QVERIFY(clock);
        QVERIFY(clock->isSingleShot());
        QVERIFY(!clock->isActive());
        list.resize(460, 180);
        auto recent = message(QStringLiteral("recent"), QStringLiteral("Now"));
        recent.setDateTime(QDateTime::currentDateTime());
        list.appendMessage(recent);
        QVERIFY(!clock->isActive()); // Creation while hidden does no polling.
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_VERIFY(clock->isActive());
        QVERIFY(clock->remainingTime() > 1000); // No more per-second wakeups.
        auto *row = list.itemWidget(0);
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        auto *body = row->findChild<ChatMessageText *>();
        QVERIFY(time && body);
        QCOMPARE(time->text(), QStringLiteral("just now"));
        QVERIFY(!time->toolTip().isEmpty());
        QVERIFY(row->toolTip().isEmpty());
        QVERIFY(body->toolTip().isEmpty());
        QVERIFY(body->viewport()->toolTip().isEmpty());
        list.hide();
        QVERIFY(!clock->isActive());
        list.show();
        QTRY_VERIFY(clock->isActive());
        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        QVERIFY(!clock->isActive());
    }

    void avatarClickRequestsContactCard()
    {
        MamChatWidget list;
        list.resize(480, 200);
        list.setGroupChat(true);
        list.setHistoryLoading(true);
        QSignalSpy requested(&list, &MamChatWidget::avatarClicked);
        auto sender = message(QStringLiteral("avatar"), QStringLiteral("Hello"));
        sender.setNick(QStringLiteral("Alice"));
        list.appendMessage(sender);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *avatar = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar"));
        QVERIFY(avatar);
        QTest::mouseClick(avatar, Qt::LeftButton);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toString(), QStringLiteral("Alice"));
        QCOMPARE(requested.at(0).at(1).toBool(), false);
        QCOMPARE(list.selectedMessageCount(), 0);
    }

    void layoutsAvatarsAndThemeSwitch()
    {
        MamChatWidget list;
        QPalette light;
        light.setColor(QPalette::Base, Qt::white);
        light.setColor(QPalette::Text, QColor(24, 24, 24));
        list.setPalette(light);
        list.resize(640, 300);
        list.setHistoryLoading(true);
        QPixmap avatar(64, 64);
        avatar.fill(Qt::blue);
        list.setAvatarResolver([avatar](const MessageView &) { return avatar; });
        auto view = message(QStringLiteral("one"), QStringLiteral("A readable message with an avatar."));
        view.setNick(QStringLiteral("Alice <admin>"));
        view.setDateTime(QDateTime::currentDateTime().addSecs(-61));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *row = list.itemWidget(0);
        auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QVERIFY(author);
        QCOMPARE(author->textFormat(), Qt::PlainText);
        QCOMPARE(author->text(), view.nick());
        QVERIFY(author->font().bold());
        QCOMPARE(author->palette().color(author->foregroundRole()), light.color(QPalette::Text));
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QCOMPARE(time->toolTip(), MamChatWidget::fullTimestamp(view.dateTime()));
        QVERIFY(row->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        if (!qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_SCREENSHOT"))
            list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_SCREENSHOT"));
        list.setMessageSelected(0, true);
        list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_VERIFY(!list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        QCOMPARE(list.selectedMessageCount(), 1);
        QCOMPARE(list.messageCount(), 1);
        list.setTheme({QStringLiteral("modern"), QStringLiteral("Modern"), NativeChatTheme::Modern, 40, 10, {}});
        QTRY_VERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar")));
    }

    void clockRefreshKeepsMaterializedRow()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        auto view = message(QStringLiteral("clock"), QStringLiteral("Time refresh"));
        // Near the first minute boundary: old 1-second polling is unnecessary,
        // but a single-shot wakeup must still update the existing row.
        view.setDateTime(QDateTime::currentDateTime().addMSecs(-58000));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QWidget *row = list.itemWidget(0);
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QVERIFY(time);
        QCOMPARE(time->text(), QStringLiteral("just now"));
        QTRY_COMPARE_WITH_TIMEOUT(time->text(), QStringLiteral("1 minute ago"), 6000);
        QCOMPARE(list.itemWidget(0), row);
    }

    void roomOverlapRequiresSameSender()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        auto alice = message(QStringLiteral("shared-id"), QStringLiteral("hello"));
        alice.setNick(QStringLiteral("Alice"));
        auto bob = alice;
        bob.setNick(QStringLiteral("Bob"));
        list.appendMessage(alice);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("bob-uid"), bob)}), 1);
        QCOMPARE(list.messageCount(), 2);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("alice-uid"), alice)}), 0);
        QCOMPARE(list.messageCount(), 2);
    }

    void encryptionLockShowsHowTheMessageTravelled()
    {
        MamChatWidget chat;
        chat.resize(560, 320);
        chat.setHistoryLoading(true);

        // A received encrypted message knows its state immediately.
        auto incoming = message("in-1", "secret");
        incoming.setEncrypted(true, "OMEMO");
        chat.appendMessage(incoming);
        // A freshly sent message: unknown until its encryption job finishes.
        chat.appendMessage(message("out-1", "pending", 1, true));
        // The plaintext outcome can arrive before the local row exists.
        MessageView plain(MessageView::MessageSecurity);
        plain.setMessageId("out-2");
        plain.setEncrypted(false);
        chat.appendMessage(plain);
        QCOMPARE(chat.messageCount(), 2); // A security event never creates a row.
        auto sent = message("out-2", "plain", 2, true);
        sent.setEdited();
        chat.appendMessage(sent);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1) && chat.itemWidget(2));

        auto *closed = chat.itemWidget(0)->findChild<QLabel *>("mamEncryptionIndicator");
        QVERIFY(closed);
        QVERIFY(closed->property("encrypted").toBool());
        QCOMPARE(closed->toolTip(), QString("Encrypted with OMEMO"));
        QVERIFY(!chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator"));

        MessageView secured(MessageView::MessageSecurity);
        secured.setMessageId("out-1");
        secured.setEncrypted(true, "OMEMO");
        chat.appendMessage(secured);
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator"));
        QVERIFY(chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator")->property("encrypted").toBool());

        auto *open = chat.itemWidget(2)->findChild<QLabel *>("mamEncryptionIndicator");
        QVERIFY(open);
        QVERIFY(!open->property("encrypted").toBool());
        QCOMPARE(open->toolTip(), QString("Not encrypted"));

        // Order: time, (checks), lock, pencil; the lock matches the pencil's
        // size and vertical position.
        MessageView delivered(MessageView::DeliveryReceipt);
        delivered.setMessageId("out-2");
        chat.appendMessage(delivered);
        QTRY_VERIFY(chat.itemWidget(2) && chat.itemWidget(2)->findChild<QLabel *>("mamDeliveryIndicator"));
        auto *row    = chat.itemWidget(2);
        auto *checks = row->findChild<QLabel *>("mamDeliveryIndicator");
        auto *lock   = row->findChild<QLabel *>("mamEncryptionIndicator");
        auto *pencil = row->findChild<QLabel *>("mamEditedIndicator");
        QVERIFY(checks && lock && pencil);
        QVERIFY(checks->geometry().right() < lock->geometry().left());
        QVERIFY(lock->geometry().right() < pencil->geometry().left());
        QCOMPARE(lock->size(), pencil->size());
        QCOMPARE(lock->geometry().top(), pencil->geometry().top());
        QCOMPARE(lock->pixmap(Qt::ReturnByValue).size(), pencil->pixmap(Qt::ReturnByValue).size());
    }

    void nativeRowsAndCorrections()
    {
        MamChatWidget list;
        list.resize(400, 250);
        list.setHistoryLoading(true);
        list.show();
        auto first = message(QStringLiteral("m1"), QStringLiteral("first body"));
        list.appendMessage(first);
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        QWidget *row = list.itemWidget(0);
        auto labels = row->findChildren<QLabel *>();
        QCOMPARE(labels.size(), 3);
        QCOMPARE(row->findChild<ChatMessageText *>()->toPlainText(), QStringLiteral("first body"));

        auto changed = message(QStringLiteral("m2"), QStringLiteral("edited body"), 2);
        changed.setReplaceId(QStringLiteral("m1"));
        list.appendMessage(changed);
        QCOMPARE(list.messageCount(), 1);
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        QTRY_COMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("edited body"));
        auto *pencil = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator"));
        QVERIFY(pencil);
        QCOMPARE(pencil->toolTip(), QStringLiteral("The message was corrected"));
        // The standalone iconset stub returns magenta for Psi assets.
        // No custom resolver is required by the production widget.
        const QPixmap actual = pencil->pixmap(Qt::ReturnByValue);
        QVERIFY(!actual.isNull());
        QCOMPARE(actual.toImage().pixelColor(0, 0), QColor(245, 10, 220));
        auto *timestamp = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QVERIFY(timestamp);
        QVERIFY(pencil->mapTo(list.itemWidget(0), QPoint()).x()
                > timestamp->mapTo(list.itemWidget(0), QPoint()).x() + timestamp->width());
        // The pencil is drawn at the timestamp font size (unscaled when
        // within a pixel of the 16 px icon) and centred on the check band:
        // the baseline up to midway between the x-height and cap height.
        // Psi's timestamp font is the chat font minus 1pt (or 1px for pixel
        // fonts). Point-to-pixel conversion depends on the environment, so
        // the near-native cases use pixel fonts.
        const auto pencilPlacement = [&list](const QFont &chatFont, int expectedSize) {
            list.setFont(chatFont);
            const auto timestampFontApplied = [&list, chatFont] {
                auto *row = list.itemWidget(0);
                auto *t   = row ? row->findChild<QLabel *>(QStringLiteral("mamTimestamp")) : nullptr;
                if (!t)
                    return false;
                return chatFont.pixelSize() > 0 ? t->font().pixelSize() == chatFont.pixelSize() - 1
                                                : qAbs(t->font().pointSizeF() - (chatFont.pointSizeF() - 1.0)) < 0.01;
            };
            if (!QTest::qWaitFor(timestampFontApplied) || !timestampFontApplied())
                return false;
            auto *row  = list.itemWidget(0);
            auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
            auto *mark = row->findChild<QLabel *>(QStringLiteral("mamEditedIndicator"));
            if (!mark)
                return false;
            const int size = expectedSize > 0 ? expectedSize : QFontInfo(time->font()).pixelSize();
            if (qAbs(size - 16) <= 1 && expectedSize <= 0)
                return true; // Covered by the explicit unscaled case.
            time->setText(QStringLiteral("xxxx"));
            auto *group = time->parentWidget();
            // Let the recreated row lay out top-down for the new font before
            // measuring; activating only the group keeps its stale size.
            QTest::qWait(50);
            if (row->layout())
                row->layout()->activate();
            QCoreApplication::processEvents();
            const QImage image = group->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            const qreal  ratio = image.devicePixelRatio();
            const QRect  rect  = mark->geometry();
            int          top = -1, bottom = -1;
            for (int y = 0; y < image.height(); ++y)
                for (int x = qRound(rect.left() * ratio); x <= qRound(rect.right() * ratio) && x < image.width(); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (c.red() > 180 && c.green() < 90 && c.blue() > 160) { // stub icon is magenta
                        if (top < 0)
                            top = y;
                        bottom = y;
                        break;
                    }
                }
            const QFontMetricsF metrics(time->font());
            const qreal baseline = time->mapTo(group, QPoint()).y() * ratio
                + ((time->contentsRect().height() - (metrics.ascent() + metrics.descent())) / 2.0 + metrics.ascent()
                   + time->contentsRect().top())
                    * ratio;
            const qreal bandCentre = baseline - (metrics.xHeight() + metrics.capHeight()) / 4.0 * ratio;
            const int   height     = bottom - top + 1;
            const qreal centre     = (top + bottom + 1) / 2.0;
            if (top < 0 || qAbs(height - qRound(size * ratio)) > 1 || qAbs(centre - bandCentre) > 1.0) {
                qWarning() << chatFont << "pencil rows" << top << bottom << "expected size" << size << "centre"
                           << centre << "band centre" << bandCentre;
                return false;
            }
            return true;
        };
        const auto pixels = [&list](int px) {
            QFont f = list.font();
            f.setPixelSize(px);
            return f;
        };
        const auto points = [&list](qreal pt) {
            QFont f = list.font();
            f.setPointSizeF(pt);
            return f;
        };
        // Timestamp fonts of 16 and 17 px keep the icon's own 16 px.
        QVERIFY2(pencilPlacement(pixels(17), 16), "Edit pencil near its native size was resized");
        QVERIFY2(pencilPlacement(pixels(18), 16), "Edit pencil near its native size was resized");
        QVERIFY2(pencilPlacement(pixels(23), 22), "Edit pencil is not font-sized and centred on the checks");
        QVERIFY2(pencilPlacement(points(20.0), 0), "Edit pencil is not font-sized and centred on the checks");
        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        QTRY_VERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
    }

    void sendingNewMessageScrollsToLiveTailButEditingDoesNot()
    {
        MamChatWidget chat;
        chat.resize(400,220);
        chat.setHistoryLoading(true);
        for (int i=0;i<44;++i)
            chat.appendMessage(message(QString::number(i),QStringLiteral("Earlier message"),i));
        chat.show();
        QTRY_VERIFY(chat.verticalScrollBar()->maximum()>0);
        chat.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QTRY_VERIFY(!chat.isAtEnd());
        auto outgoing=message(QStringLiteral("my-new"),QStringLiteral("Sent"),100,true);
        chat.appendMessage(outgoing);
        QTRY_VERIFY(chat.isAtEnd());
        QCOMPARE(chat.messageCount(),45);

        chat.scrollToIndex(0,Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(!chat.isAtEnd());
        auto corrected=message(QStringLiteral("my-edit"),QStringLiteral("Edited"),101,true);
        corrected.setReplaceId(outgoing.messageId());
        chat.appendMessage(corrected);
        QVERIFY(!chat.isAtEnd());
        QCOMPARE(chat.messageCount(),45);
    }

    void archivedCorrectionChainMergesInBothLoadOrders()
    {
        auto scenario = [this](bool archiveFirst) {
            MamChatWidget list;
            list.resize(510, 280);
            list.setHistoryLoading(true);
            list.show();
            auto original = message(QStringLiteral("a476b502-original"), QStringLiteral("6"), 1);
            auto edit7 = message(QStringLiteral("a476b502-edit7"), QStringLiteral("7"), 2);
            edit7.setReplaceId(original.messageId());
            auto edit8 = message(QStringLiteral("3f59b103-edit8"), QStringLiteral("8"), 3);
            edit8.setReplaceId(edit7.messageId());
            edit8.setNick(QStringLiteral("rion"));
            auto archived8 = edit8;
            archived8.setNick(QStringLiteral("rion@jabber.ru")); // A different display name is not a different peer.
            auto archived7 = edit7;
            archived7.setNick(QStringLiteral("rion@jabber.ru"));
            auto archived6 = original;
            archived6.setNick(QStringLiteral("rion@jabber.ru"));
            QList<QPair<QString, MessageView>> page {
                {QStringLiteral("mam-6"), archived6},
                {QStringLiteral("mam-7"), archived7},
                {QStringLiteral("mam-8"), archived8}
            };

            if (archiveFirst) {
                QCOMPARE(list.prependArchivePage(page), 1);
                list.appendMessage(edit8);
            } else {
                list.appendMessage(edit8);
                QCOMPARE(list.prependArchivePage(page), 0);
            }
            QCOMPARE(list.messageCount(), 1);
            QTRY_VERIFY(list.itemWidget(0));
            QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("8"));
            QVERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
            QCOMPARE(list.prependArchivePage(page), 0);
            list.appendMessage(edit8);
            QCOMPARE(list.messageCount(), 1);
        };
        scenario(false);
        scenario(true);
    }

    void archiveCorrectionCanFindEarlierRevisionAcrossPages()
    {
        MamChatWidget list;
        list.resize(420, 260);
        list.setHistoryLoading(true);
        auto original = message(QStringLiteral("id-6"), QStringLiteral("6"), 0);
        auto edited = message(QStringLiteral("id-7"), QStringLiteral("7"), 10);
        edited.setReplaceId(original.messageId());
        list.prependArchivePage({{QStringLiteral("uid-7"), edited}});
        QCOMPARE(list.messageCount(), 1);
        QCOMPARE(list.prependArchivePage({{QStringLiteral("uid-6"), original}}), 0);
        QCOMPARE(list.messageCount(), 1);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_COMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("7"));
    }

    void intermediateEditUnifiesPreviouslySeparateEDBAndLiveRows_data()
    {
        QTest::addColumn<bool>("retracted");
        QTest::newRow("ordinary") << false;
        QTest::newRow("stored-retraction") << true;
    }
    void intermediateEditUnifiesPreviouslySeparateEDBAndLiveRows()
    {
        QFETCH(bool, retracted);
        MamChatWidget list;
        list.resize(480, 260);
        list.setHistoryLoading(true);
        list.show();
        auto original = message(QStringLiteral("id-6"), QStringLiteral("6"), 1);
        original.setHistoryId(42);
        if (retracted)
            original.setMessageState(MessageState::Retracted);
        list.appendMessage(original); // Local EDB supplied the older version.
        auto edit8 = message(QStringLiteral("id-8"), QStringLiteral("8"), 3);
        edit8.setReplaceId(QStringLiteral("id-7"));
        list.appendMessage(edit8); // Middle revision is not loaded yet.
        QCOMPARE(list.messageCount(), 2);
        auto archive6 = original;
        archive6.setHistoryId(0);
        auto edit7 = message(QStringLiteral("id-7"), QStringLiteral("7"), 2);
        edit7.setReplaceId(original.messageId());
        QCOMPARE(list.prependArchivePage({
                     {QStringLiteral("mam-6"), archive6},
                     {QStringLiteral("mam-7"), edit7},
                     {QStringLiteral("mam-8"), edit8}}), 0);
        QCOMPARE(list.messageCount(), 1);
        QTRY_VERIFY(list.itemWidget(0));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                 retracted ? QStringLiteral("Message retracted") : QStringLiteral("8"));
        QCOMPARE(list.itemWidget(0)->property("_psiMamRowKey").toString(),
                 QStringLiteral("edb:42"));
        QVERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
    }

    void correctionCannotCrossMucSenderOrDirection()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        auto alice = message(QStringLiteral("base"), QStringLiteral("Alice"), 0);
        alice.setNick(QStringLiteral("Alice"));
        list.appendMessage(alice);
        auto bob = message(QStringLiteral("corrected"), QStringLiteral("Bob"), 2);
        bob.setNick(QStringLiteral("Bob"));
        bob.setReplaceId(QStringLiteral("base"));
        list.appendMessage(bob);
        QCOMPARE(list.messageCount(), 2);
        auto outgoing = message(QStringLiteral("another"), QStringLiteral("Outgoing"), 3, true);
        outgoing.setNick(QStringLiteral("Alice"));
        outgoing.setReplaceId(QStringLiteral("base"));
        list.appendMessage(outgoing);
        QCOMPARE(list.messageCount(), 3);
    }

    void liveArchiveOverlap()
    {
        MamChatWidget list;
        list.resize(400, 260);
        list.setHistoryLoading(true);
        list.show();
        auto live = message(QStringLiteral("live-1"), QStringLiteral("same body"), 5, true);
        list.appendMessage(live);
        QCOMPARE(list.messageCount(), 1);
        const auto archive = message(QStringLiteral("live-1"),
                                     QStringLiteral("same body"), 7, true);
        QList<QPair<QString, MessageView>> page;
        page.append(qMakePair(QStringLiteral("archive-uid-1"), archive));
        QCOMPARE(list.prependArchivePage(page), 0);
        QCOMPARE(list.messageCount(), 1);
        QCOMPARE(list.prependArchivePage(page), 0);
        QCOMPARE(list.messageCount(), 1);
        // A repeated live echo after MAM adoption must not duplicate the row.
        list.appendMessage(live);
        QCOMPARE(list.messageCount(), 1);

        auto unrelated = message(QStringLiteral("live-1"),
                                 QStringLiteral("same body"), 7, false);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid-2"), unrelated)}), 1);
        QCOMPARE(list.messageCount(), 2);
    }

    void pagesMaterializeRealWidgetsWithoutAllRows()
    {
        MamChatWidget list;
        list.resize(420, 280);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i != 250; ++i)
            list.appendMessage(message(QStringLiteral("id-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        list.scrollToIndex(120, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(120) != nullptr);
        QVERIFY(list.materializedCount() <= list.materializationLimit());
        QCOMPARE(list.messageCount(), 250);

        QList<QPair<QString, MessageView>> older;
        for (int i = 0; i != 40; ++i)
            older.append(qMakePair(QStringLiteral("uid-%1").arg(i),
                                  message(QStringLiteral("old-%1").arg(i),
                                          QStringLiteral("earlier-%1").arg(i), -100 + i)));
        QCOMPARE(list.prependArchivePage(older), 40);
        QCOMPARE(list.messageCount(), 290);
        QTRY_VERIFY(list.itemWidget(160) != nullptr);
        QCOMPARE(list.materializedCount() <= list.materializationLimit(), true);
    }

    void copySelectionAcrossUnmaterializedRows()
    {
        MamChatWidget list;
        list.resize(440, 260);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i != 240; ++i)
            list.appendMessage(message(QStringLiteral("id-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        list.selectMessageRange(12, 10); // Reversed drag also selects chronological order.
        QCOMPARE(list.selectedMessageCount(), 3);
        list.scrollToIndex(200, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(200) != nullptr);
        QVERIFY(list.itemWidget(11) == nullptr); // The source, not widgets, owns selection.
        list.scrollToIndex(11, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(11));
        auto *restored = list.itemWidget(11)->findChild<QCheckBox *>();
        QVERIFY(restored && restored->isVisible() && restored->isChecked());
        list.copySelectedMessages();
        const QString clipboard = QApplication::clipboard()->text();
        QVERIFY(clipboard.contains(QStringLiteral("body-10")));
        QVERIFY(clipboard.contains(QStringLiteral("body-11")));
        QVERIFY(clipboard.contains(QStringLiteral("body-12")));
        QVERIFY(clipboard.indexOf(QStringLiteral("body-10"))
                < clipboard.indexOf(QStringLiteral("body-11")));
        QVERIFY(clipboard.indexOf(QStringLiteral("body-11"))
                < clipboard.indexOf(QStringLiteral("body-12")));
        QVERIFY(!clipboard.contains(QStringLiteral("body-13")));
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("uid-old"),
                          message(QStringLiteral("old"), QStringLiteral("earlier"), -10))}), 1);
        QCOMPARE(list.selectedMessageCount(), 3);
        list.copySelectedMessages();
        QCOMPARE(QApplication::clipboard()->text(), clipboard);
        auto *toolbar = list.findChild<QFrame *>(QStringLiteral("mamSelectionToolbar"));
        QVERIFY(toolbar);
        QVERIFY(!toolbar->isHidden());
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(toolbar->isHidden());
    }

    void archiveAdoptionPreservesSelectedLiveRow()
    {
        MamChatWidget list;
        list.resize(420, 280);
        list.setHistoryLoading(true);
        list.show();
        const auto live = message(QStringLiteral("same"), QStringLiteral("body"), 1, true);
        list.appendMessage(live);
        list.setMessageSelected(0, true);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid"),
           message(QStringLiteral("same"), QStringLiteral("body"), 2, true))}), 0);
        QCOMPARE(list.selectedMessageCount(), 1);
        list.copySelectedMessages();
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("body")));
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid"),
           message(QStringLiteral("same"), QStringLiteral("body"), 2, true))}), 0);
        QCOMPARE(list.selectedMessageCount(), 1);
    }

    void mouseSelectionAndCopyToolbar()
    {
        MamChatWidget list;
        list.resize(430, 520);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QStringLiteral("m-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        QWidget *r0 = list.itemWidget(0);
        QWidget *r1 = list.itemWidget(1);
        QWidget *r2 = list.itemWidget(2);
        QVERIFY(r0);
        QVERIFY(r1);
        QVERIFY(r2);
        const auto labels0 = r0->findChildren<QLabel *>();
        const auto labels1 = r1->findChildren<QLabel *>();
        const auto labels2 = r2->findChildren<QLabel *>();
        QVERIFY(!labels0.isEmpty());
        QVERIFY(!labels1.isEmpty());
        QVERIFY(!labels2.isEmpty());
        auto *first = labels0.first();
        auto *second = labels1.first();
        auto *third = labels2.first();
        // mapTo(QWidget*) requires an ancestor, not a sibling QLabel.
        // Capture the target through global coordinates before virtualization
        // has a chance to recycle either of the row widgets.
        const QPoint dragTarget =
            first->mapFromGlobal(third->mapToGlobal(QPoint(10, 10)));
        QTest::mouseClick(first, Qt::LeftButton, Qt::ControlModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 1);
        QTest::mouseClick(third, Qt::LeftButton, Qt::ShiftModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 3);
        QTest::mouseClick(second, Qt::LeftButton, Qt::ControlModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 2);
        list.copySelectedMessages();
        const QString copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains(QStringLiteral("body-0")));
        QVERIFY(!copied.contains(QStringLiteral("body-1")));
        QVERIFY(copied.contains(QStringLiteral("body-2")));
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);

        // A normal drag wholly within one row remains text selection.
        // Qt5 and Qt6 QTest::mouseMove differ in whether a held mouse button
        // is reflected in the synthetic event. Deliver the same real gesture
        // sequence with explicit button state in both Qt versions.
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(3, 3),
                          first->mapTo(first->window(), (QPointF(3, 3)).toPoint()), first->mapToGlobal((QPointF(3, 3)).toPoint()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &press);
        QMouseEvent inside(QEvent::MouseMove, QPointF(30, 3),
                          first->mapTo(first->window(), (QPointF(30, 3)).toPoint()), first->mapToGlobal((QPointF(30, 3)).toPoint()),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &inside);
        QCOMPARE(list.selectedMessageCount(), 0);

        // Leaving the originating message row switches to whole-post range.
        QMouseEvent outside(QEvent::MouseMove, QPointF(dragTarget),
                          first->mapTo(first->window(), (QPointF(dragTarget)).toPoint()), first->mapToGlobal((QPointF(dragTarget)).toPoint()),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &outside);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(dragTarget),
                          first->mapTo(first->window(), (QPointF(dragTarget)).toPoint()), first->mapToGlobal((QPointF(dragTarget)).toPoint()),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(first, &release);
        QCOMPARE(list.selectedMessageCount(), 3);
        list.copySelectedMessages();
        const QString full = QApplication::clipboard()->text();
        QVERIFY(full.contains(QStringLiteral("body-0")));
        QVERIFY(full.contains(QStringLiteral("body-1")));
        QVERIFY(full.contains(QStringLiteral("body-2")));
    }

    void wheelScrollingDuringDragExtendsMessageSelection()
    {
        MamChatWidget list;
        list.resize(430, 300);
        list.setHistoryLoading(true);
        for (int i = 0; i < 40; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("body-%1").arg(i), i));
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list));
        QTRY_VERIFY(list.itemWidget(39));

        // Press through the window system so QGuiApplication::mouseButtons()
        // reflects the held button, exactly as during a real drag.
        QWidget *row = list.itemWidget(39);
        const QPoint global = row->mapToGlobal(QPoint(row->width() / 3, row->height() / 2));
        QWindow *window = list.window()->windowHandle();
        QVERIFY(window);
        const QPoint local = list.window()->mapFromGlobal(global);
        QCursor::setPos(global);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, local);
        QCOMPARE(list.selectedMessageCount(), 0);

        // Scroll older rows under the stationary pointer, with no MouseMove.
        QScrollBar *bar = list.verticalScrollBar();
        const int scrolled = 3 * row->height();
        bar->setValue(bar->value() - scrolled);
        QTRY_VERIFY(list.selectedMessageCount() > 1);
        const int selected = list.selectedMessageCount();

        // Scroll far enough that the pressed row is evicted (hidden and
        // deleted). The gesture must survive losing Qt's implicit grab.
        bar->setValue(bar->minimum());
        QCoreApplication::processEvents();
        QTRY_VERIFY(list.selectedMessageCount() > selected);
        QTRY_VERIFY(!list.itemWidget(39));
        const int afterJump = list.selectedMessageCount();

        // Continue the same gesture by moving the pointer further up.
        const QPoint higher(local.x(), qMax(5, local.y() - scrolled));
        QVERIFY(higher.y() < local.y());
        QCursor::setPos(list.window()->mapToGlobal(higher));
        QTest::mouseMove(window, higher);
        QTRY_VERIFY(list.selectedMessageCount() > afterJump);

        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, higher);
        const int finalCount = list.selectedMessageCount();
        QVERIFY(QWidget::mouseGrabber() != list.viewport());

        // Once the button is up, neither scrolling nor hovering extends it.
        bar->setValue(bar->value() + scrolled);
        QCoreApplication::processEvents();
        QCOMPARE(list.selectedMessageCount(), finalCount);
        QCursor::setPos(global);
        QTest::mouseMove(window, local);
        QCoreApplication::processEvents();
        QCOMPARE(list.selectedMessageCount(), finalCount);
    }

    void clearingConversationDiscardsPlainClickSelectionAnchor()
    {
        MamChatWidget list;
        list.resize(430, 500);
        list.setHistoryLoading(true);
        list.show();
        list.appendMessage(message(QStringLiteral("old"),
                                   QStringLiteral("old-body"), 0));
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        const auto oldLabels = list.itemWidget(0)->findChildren<QLabel *>();
        QVERIFY(!oldLabels.isEmpty());

        // An ordinary click does not select a whole row, but records a
        // Shift-selection anchor. The next chat reuses generated row keys.
        QTest::mouseClick(oldLabels.first(), Qt::LeftButton,
                          Qt::NoModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 0);
        list.clearMessages();

        list.appendMessage(message(QStringLiteral("new-1"),
                                   QStringLiteral("first-new"), 1));
        list.appendMessage(message(QStringLiteral("new-2"),
                                   QStringLiteral("second-new"), 2));
        QTRY_VERIFY(list.itemWidget(1) != nullptr);
        const auto newLabels = list.itemWidget(1)->findChildren<QLabel *>();
        QVERIFY(!newLabels.isEmpty());
        QTest::mouseClick(newLabels.first(), Qt::LeftButton,
                          Qt::ShiftModifier, QPoint(3, 3));

        // Without clearing the old anchor, this accidentally selects both
        // new messages because live:generated:1 is reused.
        QCOMPARE(list.selectedMessageCount(), 1);
        list.copySelectedMessages();
        const auto copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains(QStringLiteral("second-new")));
        QVERIFY(!copied.contains(QStringLiteral("first-new")));
    }

    void shortArchivePagesDoNotAutoPageAtTheLiveTail()
    {
        MamChatWidget list;
        list.resize(360, 240);
        list.setHistoryLoading(true);
        list.show();
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);

        // Protocol-only MUC pages can leave the native list short (or empty).
        // Completing a page must not implicitly fetch the entire room archive.
        for (int page = 0; page < 4; ++page) {
            list.setHistoryLoading(false);
            QCoreApplication::processEvents();
            QCOMPARE(requests.count(), 0);
            QVERIFY(list.isAtEnd());
            list.setHistoryLoading(true);
        }

        // Replaying a small real page also must not trigger a background sweep.
        list.prependArchivePage({qMakePair(QStringLiteral("uid-1"),
                                          message(QStringLiteral("id-1"), QStringLiteral("Short reply")))});
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 0);
        QVERIFY(list.isAtEnd());
    }

    void initialViewportFillContinuesThroughLongProtocolOnlyRuns()
    {
        MamChatWidget room;
        room.resize(480, 900); // A short MUC backlog leaves ample empty space.
        room.setHistoryLoading(true);
        for (int i = 0; i < 4; ++i)
            room.appendMessage(message(QStringLiteral("live-%1").arg(i),
                                       QStringLiteral("Recent MUC body"), i));
        room.show();
        room.setHistoryLoading(false);
        QSignalSpy requests(&room, &MamChatWidget::olderMessagesRequested);

        QTest::qWait(70);
        QCOMPARE(requests.count(), 0); // A passive layout update grants nothing.

        room.maybeContinueInitialHistoryFill(); // First HistorySync page was handled.
        QTRY_COMPARE(requests.count(), 1);
        QCOMPARE(room.messageCount(), 4); // The already delivered MUC messages survive.

        // More than the previous 8/32-page caps: protocol-only pages advance
        // the source cursor but cannot satisfy the view's real-message demand.
        for (int i = 2; i <= 36; ++i) {
            room.setHistoryLoading(true);
            room.setHistoryLoading(false);
            room.maybeContinueInitialHistoryFill();
            QTRY_COMPARE(requests.count(), i);
        }
        room.setHistoryExhausted(true);
        room.setHistoryLoading(false);
        room.maybeContinueInitialHistoryFill();
        QTest::qWait(50);
        QCOMPARE(requests.count(), 36);
        QCOMPARE(room.messageCount(), 4);
    }

    void scrollFillPreservesAnchorAndContinuesPastProtocolOnlyPages()
    {
        MamChatWidget chat;
        chat.resize(420, 300);
        for (int i = 0; i < 30; ++i)
            chat.appendMessage(message(QString::number(i), "Message body", i));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(29));
        chat.scrollToIndex(0, Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(chat.itemWidget(0));
        auto       *anchor = chat.itemWidget(0);
        const int   oldY   = anchor->y();
        QSignalSpy  requests(&chat, &MamChatWidget::olderMessagesRequested);
        QWheelEvent wheel(QPointF(5, 5), QPointF(chat.viewport()->mapToGlobal(QPoint(5, 5))), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(chat.viewport(), &wheel);
        QTRY_COMPARE(requests.count(), 1);
        for (int page = 2; page <= 4; ++page) {
            chat.setHistoryLoading(true);
            chat.setHistoryLoading(false);
            chat.maybeContinueInitialHistoryFill(); // Distinct progressing source cursors, no visible rows.
            QTRY_COMPARE(requests.count(), page);
        }
        chat.setHistoryLoading(true);
        QList<QPair<QString, MessageView>> older;
        for (int i = 0; i < 6; ++i)
            older.append(qMakePair(QString("old-%1").arg(i), message(QString("old-%1").arg(i), "Older body", i - 20)));
        chat.prependArchivePage(older);
        chat.setHistoryLoading(false);
        chat.maybeContinueInitialHistoryFill();
        QTest::qWait(60);
        QCOMPARE(requests.count(), 4); // Half a screen, rather than an archive sweep.
        QCOMPARE(chat.itemWidget(6), anchor);
        QCOMPARE(anchor->y(), oldY);
    }

    void roomArchiveIdentityMergesJoinHistoryWithDifferentClientIds()
    {
        MamChatWidget chat;
        chat.setGroupChat(true);
        auto joined = message("join-client-id", "Body");
        joined.setArchiveId("room-stanza-id");
        chat.appendMessage(joined);
        auto archived = joined;
        archived.setMessageId("rewritten-client-id");
        QCOMPARE(chat.prependArchivePage({ qMakePair(QString("room-stanza-id"), archived) }), 0);
        QCOMPARE(chat.messageCount(), 1);
    }

    void liveMessageSurvivesInitialFillAndRemainsAtTail()
    {
        MamChatWidget chat;
        chat.resize(420, 600);
        for (int i = 0; i < 4; ++i)
            chat.appendMessage(message(QString("join-%1").arg(i), "MUC history", i));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(3));
        QSignalSpy requests(&chat, &MamChatWidget::olderMessagesRequested);
        chat.maybeContinueInitialHistoryFill();
        QTRY_COMPARE(requests.count(), 1);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("live-in-flight", "New live body", 20));
        QList<QPair<QString, MessageView>> older;
        for (int i = 0; i < 30; ++i)
            older.append(qMakePair(QString("old-%1").arg(i), message(QString("old-%1").arg(i), "Older body", i - 40)));
        chat.prependArchivePage(older);
        chat.setHistoryLoading(false);
        chat.maybeContinueInitialHistoryFill();
        QTRY_VERIFY(chat.itemWidget(34));
        QCOMPARE(chat.messageCount(), 35);
        QCOMPARE(chat.itemWidget(34)->findChild<ChatMessageText *>()->toPlainText(), QString("New live body"));
        QVERIFY(chat.isAtEnd());
        QTest::qWait(60);
        QCOMPARE(requests.count(), 1);
    }

    void sparseScrollbarSeekLoadsRealRowsAndPreservesLiveTail()
    {
        MamChatWidget chat;
        chat.resize(420, 260);
        chat.setGroupChat(true);
        for (int i = 0; i < 4; ++i) {
            auto entry = message(QString("join-%1").arg(i), "MUC body", i);
            entry.setArchiveId(QString("join-uid-%1").arg(i));
            chat.appendMessage(entry);
        }
        QList<QPair<QString, MessageView>> tail;
        QStringList                        tailUids;
        for (int i = 0; i < 10; ++i) {
            const auto uid = QString("tail-%1").arg(i);
            tail.append(qMakePair(uid, message(uid, "Older tail body", i - 20)));
            tailUids << uid;
        }
        chat.prependArchivePage(tail);
        chat.observeArchivePage(tailUids, 1000000, 999986, "join-uid-0");
        chat.show();
        chat.maybeContinueInitialHistoryFill();
        QTRY_VERIFY(chat.historySource().isVirtual());
        QCOMPARE(chat.messageCount(), 14);
        QVERIFY(chat.itemCount() >= 999990);
        QVERIFY(chat.isAtEnd());
        QSignalSpy requests(&chat, &MamChatWidget::archiveRangeRequested);
        auto      *bar = chat.verticalScrollBar();
        bar->setSliderDown(true);
        bar->setSliderPosition(bar->maximum() / 2);
        bar->setSliderDown(false);
        QTRY_VERIFY(!requests.isEmpty());
        const auto request = qvariant_cast<MamHistorySource::Request>(requests.last().at(0));
        QCOMPARE(request.kind, MamHistorySource::Request::Index);
        QVERIFY(request.index > 450000 && request.index < 550000);
        QVERIFY(request.limit >= 15 && request.limit <= 40);
        chat.setHistoryLoading(true);
        QList<QPair<QString, MessageView>> middle;
        QStringList                        middleUids;
        for (int i = 0; i < request.limit; ++i) {
            const auto uid = QString("middle-%1").arg(i);
            middle.append(qMakePair(uid, message(uid, "Actual middle body", i - 10000)));
            middleUids << uid;
        }
        chat.prependArchivePage(middle);
        chat.observeArchivePage(middleUids, 1000000, request.index, {}, {}, false);
        chat.completeArchiveRange(request, true);
        QTRY_VERIFY(chat.itemWidget(chat.historySource().indexOfKey(QString("mam:middle-%1").arg(request.limit / 2))));
        QVERIFY(chat.messageCount() <= 54);
        QVERIFY(chat.materializedCount() <= chat.materializationLimit());
        QTRY_VERIFY(chat.itemWidget(chat.viewportCenterIndex()));
        auto *shown = chat.itemWidget(chat.viewportCenterIndex());
        QVERIFY(shown->findChild<ChatMessageText *>()->toPlainText().contains("Actual middle body"));
        chat.appendMessage(message("live-at-tail", "Live while reading middle", 100));
        QVERIFY(!chat.isAtEnd());
        QCOMPARE(chat.historySource().rows().last().message.messageId(), QString("live-at-tail"));
    }

    void sparseSeekSurvivesDensityShrinkAcrossProtocolOnlyPage()
    {
        MamChatWidget chat;
        chat.resize(420, 260);
        QStringList                        uids;
        QList<QPair<QString, MessageView>> tail;
        for (int i = 0; i < 14; ++i) {
            const auto uid = QString("tail-%1").arg(i);
            uids << uid;
            tail << qMakePair(uid, message(uid, "Tail body", i));
        }
        chat.prependArchivePage(tail);
        chat.observeArchivePage(uids, 1000000, 999986);
        chat.show();
        chat.maybeContinueInitialHistoryFill();
        QTRY_VERIFY(chat.historySource().isVirtual());
        QSignalSpy requests(&chat, &MamChatWidget::archiveRangeRequested);
        auto      *bar = chat.verticalScrollBar();
        bar->setSliderDown(true);
        bar->setSliderPosition(bar->maximum() / 2);
        bar->setSliderDown(false);
        QTRY_VERIFY(!requests.isEmpty());
        auto request = qvariant_cast<MamHistorySource::Request>(requests.last().at(0));
        QVERIFY(request.generation > 0);
        QCOMPARE(request.kind, MamHistorySource::Request::Index);
        const auto generation = request.generation;
        const int  middle     = request.index;
        int        firstEvent = middle;
        for (int page = 0; page < 4; ++page) {
            QStringList receipts;
            for (int i = 0; i < request.limit; ++i)
                receipts << QString("receipt-%1-%2").arg(page).arg(i);
            chat.observeArchivePage(receipts, 1000000, firstEvent, {}, {}, false);
            const int previousRequests = requests.size();
            chat.completeArchiveRange(request, true, {}, false);
            QTRY_VERIFY(requests.size() > previousRequests);
            request = qvariant_cast<MamHistorySource::Request>(requests.last().at(0));
            QCOMPARE(request.generation, generation);
            QCOMPARE(request.kind, MamHistorySource::Request::Before);
            firstEvent -= request.limit;
        }
        QVERIFY(chat.itemCount() < 500000); // Old seek slot is now outside the list.
        const int  physicalRequests = requests.size();
        QSignalSpy cancelled(&chat, &MamChatWidget::archiveRangeCancelled);
        // Generic layout may request a far new logical range after density
        // refinement. It must attach to the same in-flight user navigation.
        chat.rangeRequested(0, 3, Mattermost::LongListWidget::RequestReason::Scroll, 0);
        QCOMPARE(requests.size(), physicalRequests);
        QCOMPARE(cancelled.size(), 0);
        QList<QPair<QString, MessageView>> bodies;
        QStringList                        bodyUids;
        for (int i = 0; i < request.limit; ++i) {
            const auto uid = QString("middle-body-%1").arg(i);
            bodyUids << uid;
            bodies << qMakePair(uid, message(uid, "Middle after receipts", i - 100));
        }
        chat.prependArchivePage(bodies);
        chat.observeArchivePage(bodyUids, 1000000, firstEvent, request.cursor, {}, false);
        chat.completeArchiveRange(request, true);
        QTRY_VERIFY(chat.itemWidget(chat.viewportCenterIndex()));
        QVERIFY(chat.itemWidget(chat.viewportCenterIndex())
                    ->findChild<ChatMessageText *>()
                    ->toPlainText()
                    .contains("Middle after receipts"));
    }

    void rejectedIndexCollapsesToUsableCursorHistory()
    {
        MamChatWidget chat;
        chat.resize(420, 260);
        QStringList                        uids;
        QList<QPair<QString, MessageView>> page;
        for (int i = 0; i < 15; ++i) {
            const auto uid = QString("tail-%1").arg(i);
            uids << uid;
            page << qMakePair(uid, message(uid, "Real cached body", i));
        }
        chat.prependArchivePage(page);
        chat.observeArchivePage(uids, 10000, 9985);
        chat.show();
        chat.maybeContinueInitialHistoryFill();
        QTRY_VERIFY(chat.historySource().isVirtual());
        QSignalSpy requests(&chat, &MamChatWidget::archiveRangeRequested);
        chat.scrollToIndex(5000, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(!requests.isEmpty());
        const auto request = qvariant_cast<MamHistorySource::Request>(requests.last().at(0));
        QSignalSpy fallback(&chat, &MamChatWidget::archiveSeekUnavailable);
        chat.completeArchiveRange(request, false, "feature-not-implemented");
        QTRY_COMPARE(chat.itemCount(), 15);
        QCOMPARE(chat.messageCount(), 15);
        QCOMPARE(fallback.count(), 1);
        QVERIFY(!chat.historySource().canSeek());
        chat.scrollToIndex(0, Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("Real cached body"));
    }

    void pageSizeUsesVisibleMessageHeightsRatherThanArchiveEventCount()
    {
        MamChatWidget room;
        room.resize(420, 900);
        room.setHistoryLoading(true);
        for (int i = 0; i < 4; ++i)
            room.appendMessage(message(QString::number(i), QStringLiteral("Normal MUC body"), i));
        room.show();
        QTRY_VERIFY(room.itemWidget(0) != nullptr);
        const int ordinarySize = room.suggestedOlderHistoryPageSize();
        QVERIFY(ordinarySize >= 15 && ordinarySize <= 40);

        // The opening request covers the missing screen plus an extra half
        // screen; each message has three estimated protocol events.
        room.maybeContinueInitialHistoryFill();
        const int openingSize = room.suggestedOlderHistoryPageSize();
        QVERIFY(openingSize >= ordinarySize);
        QVERIFY(openingSize <= 40);
        QVERIFY(openingSize == 40 || openingSize % 3 == 0);

        MamChatWidget small;
        small.resize(420, 120);
        QCOMPARE(small.suggestedOlderHistoryPageSize(), 15);
    }

    void initialViewportFillStopsWhenRealRowsFillWindow()
    {
        MamChatWidget room;
        room.resize(420, 220);
        room.setHistoryLoading(true);
        for (int i = 0; i < 24; ++i)
            room.appendMessage(message(QStringLiteral("live-%1").arg(i),
                                       QStringLiteral("Normal MUC body"), i));
        room.show();
        QTRY_VERIFY(room.verticalScrollBar()->maximum() > 0);
        room.setHistoryLoading(false);
        QSignalSpy requests(&room, &MamChatWidget::olderMessagesRequested);

        room.maybeContinueInitialHistoryFill();
        QTest::qWait(90);
        QCOMPARE(requests.count(), 0);
        room.resize(420, 420);
        QTest::qWait(90);
        QCOMPARE(requests.count(), 0); // No resize-triggered archive sweep.
    }

    void scrollBarOutwardActionAtOldestBoundaryFetchesHistory()
    {
        MamChatWidget room;
        room.resize(360, 180);
        room.setHistoryLoading(true);
        for (int i = 0; i < 30; ++i)
            room.appendMessage(message(QString::number(i), QStringLiteral("Visible body"), i));
        room.show();
        QTRY_VERIFY(room.verticalScrollBar()->maximum() > 0);
        room.setHistoryLoading(false);
        QSignalSpy requests(&room, &MamChatWidget::olderMessagesRequested);

        auto *bar = room.verticalScrollBar();
        bar->setValue(bar->minimum());
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 0); // Programmatic position changes are passive.
        // First showEvent may still finish tail restoration; establish the
        // top boundary *after* those queued layout operations.
        bar->setValue(bar->minimum());
        QCOMPARE(bar->value(), bar->minimum());
        // At zero, QAbstractSlider::triggerAction(SingleStepSub) can be
        // discarded by Qt itself. Send genuine input to exercise the widget's
        // outward-gesture path rather than relying on a valueChanged signal.
        QWheelEvent upward(QPointF(4, 4), QPointF(bar->mapToGlobal(QPoint(4, 4))),
                           QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                           Qt::NoScrollPhase, false);
        QApplication::sendEvent(bar, &upward);
        QTRY_COMPARE(requests.count(), 1); // The value cannot decrease below zero.
        room.setHistoryLoading(true);
        room.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 1);
        QTest::keyClick(bar, Qt::Key_PageUp);
        QTRY_COMPARE(requests.count(), 2); // New, distinct user gesture.
    }

    void scrollingFromTailToOldestRequestsHistory()
    {
        MamChatWidget list;
        list.resize(400, 240);
        list.setHistoryLoading(true);
        for (int i = 0; i < 30; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Visible body"), i));
        list.show();
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        list.setHistoryLoading(false);
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);
        QVERIFY(list.isAtEnd());
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 0);

        list.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QTRY_VERIFY(!list.isAtEnd());
        QTRY_COMPARE(requests.count(), 1);
    }

    void smallScrollNearTailDoesNotRequestHistoryOrChainOnPrepend()
    {
        MamChatWidget list;
        list.resize(400, 180);
        list.setHistoryLoading(true);
        for (int i = 0; i < 35; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Visible body"), i));
        list.show();
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        list.setHistoryLoading(false);
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);
        QVERIFY(list.isAtEnd());

        const auto wheelUp = [&list] {
            QWheelEvent event(QPointF(4, 4), QPointF(list.viewport()->mapToGlobal(QPoint(4, 4))),
                              QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);
            QApplication::sendEvent(list.viewport(), &event);
        };
        wheelUp(); // One wheel tick at the live tail is not an archive boundary.
        QCoreApplication::processEvents();
        QVERIFY(list.visibleRange().first > 1);
        QCOMPARE(requests.count(), 0);

        // Programmatic navigation/geometry must not masquerade as scroll intent.
        list.verticalScrollBar()->setValue(list.verticalScrollBar()->minimum());
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 0);

        wheelUp(); // Explicit outward gesture at the oldest row.
        QTRY_COMPARE(requests.count(), 1);
        list.setHistoryLoading(true);
        list.prependArchivePage({qMakePair(QStringLiteral("archive-old-1"),
                                          message(QStringLiteral("old-1"), QStringLiteral("An older reply")))});
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 1); // Prepending a short page cannot chain requests.

        // A subsequent, separate upward gesture is allowed to page again.
        wheelUp();
        QTRY_COMPARE(requests.count(), 2);
    }

    void cachedPageCompletionCannotReplaySameWheelGesture()
    {
        MamChatWidget list;
        list.resize(400, 240);
        list.setHistoryLoading(true);
        for (int i = 0; i < 35; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Visible body"), i));
        list.show();
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 100);
        list.setHistoryLoading(false);
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);

        // Start just below the oldest boundary. One wheel both moves the
        // scrollbar and requests a page. The deferred scrollbar check must not
        // reuse that same gesture after a cached HistorySync reply completes.
        QCoreApplication::processEvents(); // Drain initial show/tail restoration.
        list.verticalScrollBar()->setValue(20);
        QCOMPARE(list.verticalScrollBar()->value(), 20);
        QWheelEvent event(QPointF(4, 4), QPointF(list.viewport()->mapToGlobal(QPoint(4, 4))),
                          QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(list.viewport(), &event);
        QCOMPARE(requests.count(), 1);
        list.setHistoryLoading(true);
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 1);
    }

    void upwardWheelAtEmptyOrShortHistoryRequestsOnePagePerGesture()
    {
        MamChatWidget list;
        list.resize(360, 240);
        list.setHistoryLoading(true);
        list.show();
        list.setHistoryLoading(false);
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);

        const auto wheel = [&list](int delta) {
            QWheelEvent event(QPointF(4, 4), QPointF(list.viewport()->mapToGlobal(QPoint(4, 4))),
                              QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);
            QApplication::sendEvent(list.viewport(), &event);
        };
        // Scrolling down at the live tail is not an older-history request.
        wheel(-120);
        QCOMPARE(requests.count(), 0);
        wheel(120);
        QCOMPARE(requests.count(), 1);
        wheel(120); // A second gesture before the queued slot must not duplicate a request.
        QCOMPARE(requests.count(), 1);

        // Loading/completion alone must not queue subsequent MAM requests.
        list.setHistoryLoading(true);
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 1);
        wheel(120);
        QCOMPARE(requests.count(), 2);

        list.setHistoryExhausted(true);
        wheel(120);
        QCOMPARE(requests.count(), 2);
    }
};

QTEST_MAIN(MamChatWidgetTest)
#include "MamChatWidgetTest.moc"
