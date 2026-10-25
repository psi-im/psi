// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only style plugin: exercises the real application without production test hooks.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleFactory>
#include <QStylePlugin>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QtTest>

class Driver : public QObject {
public:
    QTimer            timer;
    QElapsedTimer     elapsed;
    int               stage   = 0;
    bool              bounced = false;
    QPointer<QWidget> options, chat;
    explicit Driver(QObject *parent) : QObject(parent)
    {
        elapsed.start();
        QObject::connect(&timer, &QTimer::timeout, this, [this] { step(); });
        timer.start(50);
    }
    void fail(const char *why)
    {
        qCritical("UI-PROBE FAIL: %s stage=%d", why, stage);
        timer.stop();
        QApplication::exit(2);
    }
    bool contains(const QAbstractItemModel *model, const QModelIndex &parent, const QString &text, int depth = 0)
    {
        if (depth > 4)
            return false;
        for (int row = 0; row < model->rowCount(parent); ++row) {
            auto index = model->index(row, 0, parent);
            if (model->data(index).toString().contains(text))
                return true;
            if (contains(model, index, text, depth + 1))
                return true;
        }
        return false;
    }
    QModelIndex findPeer(const QAbstractItemModel *model, const QModelIndex &parent = {}, int depth = 0)
    {
        if (depth > 4)
            return {};
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const auto index = model->index(row, 0, parent);
            if (model->data(index).toString() == "UI Test Peer")
                return index;
            const auto child = findPeer(model, index, depth + 1);
            if (child.isValid())
                return child;
        }
        return {};
    }
    void disable()
    {
        auto *enabled = options->findChild<QCheckBox *>("serverlessEnabled");
        enabled->setChecked(false);
        QMetaObject::invokeMethod(options, "doApply", Qt::DirectConnection);
        if (!bounced) {
            bounced = true;
            enabled->setChecked(true);
            QMetaObject::invokeMethod(options, "doApply", Qt::DirectConnection);
            stage = 7;
            return;
        }
        stage = 6;
    }
    void step()
    {
        if (elapsed.elapsed() > 40000) {
            fail("Timed out");
            return;
        }
        if (stage == 0) {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (widget->inherits("MainWin")) {
                    QMetaObject::invokeMethod(widget, "doOptions", Qt::QueuedConnection);
                    stage = 1;
                    return;
                }
            }
        } else if (stage == 1) {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (!widget->inherits("OptionsDlg"))
                    continue;
                options = widget;
                for (auto *list : widget->findChildren<QListWidget *>())
                    for (int row = 0; row < list->count(); ++row)
                        if (list->item(row)->data(Qt::UserRole).toString() == "accounts")
                            list->setCurrentRow(row);
                for (auto *tabs : widget->findChildren<QTabWidget *>()) {
                    for (int index = 0; index < tabs->count(); ++index) {
                        if (tabs->tabText(index) == "Serverless") {
                            tabs->setCurrentIndex(index);
                            stage = 2;
                            return;
                        }
                    }
                }
            }
        } else if (stage == 2) {
            auto *enabled   = options->findChild<QCheckBox *>("serverlessEnabled");
            auto *name      = options->findChild<QLineEdit *>("serverlessName");
            auto *networkInterface = options->findChild<QComboBox *>("serverlessInterface");
            if (!enabled || !name || !networkInterface) {
                fail("Settings fields missing");
                return;
            }
            if (!enabled->isEnabled()) {
                fail("Serverless not supported");
                return;
            }
            if (name->text() != qEnvironmentVariable("PSI_UI_EXPECTED_NAME", "Saved user")) {
                fail("Name not restored from profile");
                return;
            }
            const int index = networkInterface->findData("lo");
            if (index < 0) {
                fail("Loopback test interface missing");
                return;
            }
            name->setText("UI Alice");
            networkInterface->setCurrentIndex(index);
            enabled->setChecked(true);
            QMetaObject::invokeMethod(options, "doApply", Qt::QueuedConnection);
            stage = 3;
        } else if (stage == 3) {
            auto *status = options->findChild<QLabel *>("serverlessStatus");
            if (!status || status->text() != "Online on the local network")
                return;
            bool roster = false;
            for (auto *widget : QApplication::topLevelWidgets())
                if (widget->inherits("MainWin"))
                    for (auto *view : widget->findChildren<QTreeView *>())
                        if (view->model() && contains(view->model(), {}, "Local network"))
                            roster = true;
            if (!roster) {
                fail("Serverless account missing from roster model");
                return;
            }
            options->grab().save(qEnvironmentVariable("PSI_UI_SCREENSHOT"));
            qInfo("UI-PROBE ONLINE: settings applied and account visible in roster");
            if (!qEnvironmentVariableIsSet("PSI_UI_CHAT_TEST")) {
                disable();
                return;
            }
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (!widget->inherits("MainWin"))
                    continue;
                for (auto *view : widget->findChildren<QTreeView *>()) {
                    if (!view->model() || !view->isVisible())
                        continue;
                    const auto peer = findPeer(view->model());
                    if (!peer.isValid())
                        continue;
                    options->hide();
                    view->expandAll();
                    view->setCurrentIndex(peer);
                    view->scrollTo(peer);
                    view->setFocus();
                    QTest::keyClick(view, Qt::Key_Return);
                    stage = 4;
                    return;
                }
            }
        } else if (stage == 4) {
            for (auto *widget : QApplication::allWidgets()) {
                if (!widget->inherits("ChatDlg"))
                    continue;
                chat = widget;
                for (auto *editor : widget->findChildren<QTextEdit *>()) {
                    if (editor->isReadOnly())
                        continue;
                    editor->setPlainText("serverless-ui-test");
                    auto *send = widget->findChild<QPushButton *>("pb_send");
                    if (!send) {
                        fail("Chat send action missing");
                        return;
                    }
                    send->click();
                    stage = 5;
                    return;
                }
            }
        } else if (stage == 5) {
            if (!chat) {
                fail("Chat disappeared");
                return;
            }
            int copies = 0;
            for (auto *text : chat->findChildren<QTextEdit *>())
                if (text->isReadOnly() && text->inherits("ChatMessageText")
                    && text->toPlainText().contains("serverless-ui-test"))
                    ++copies;
            if (copies < 2)
                return;
            qInfo("UI-PROBE MESSAGES: outgoing and echoed message rendered in the native chat");
            disable();
        } else if (stage == 7) {
            auto *status = options->findChild<QLabel *>("serverlessStatus");
            if (!status || status->text() != "Online on the local network")
                return;
            qInfo("UI-PROBE RESTART: a quick disable/enable waits for shutdown and reconnects");
            disable();
        } else if (stage == 6) {
            auto *status = options->findChild<QLabel *>("serverlessStatus");
            if (!status || status->text() != "Disabled")
                return;
            timer.stop();
            qInfo("UI-PROBE PASS: disabled account preserved for history and settings");
            QTimer::singleShot(1000, qApp, [this] {
                if (options)
                    options->close();
                for (auto *widget : QApplication::topLevelWidgets())
                    if (widget->inherits("MainWin"))
                        QMetaObject::invokeMethod(widget, "closeProgram", Qt::QueuedConnection);
            });
        }
    }
};
class ProbePlugin : public QStylePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "probe.json")
public:
    QStyle *create(const QString &key) override
    {
        if (key.toLower() != "serverless-ui-probe")
            return nullptr;
        QTimer::singleShot(0, qApp, [] { new Driver(qApp); });
        return new QProxyStyle(QStyleFactory::create("Fusion"));
    }
};
#include "gui_driver.moc"
