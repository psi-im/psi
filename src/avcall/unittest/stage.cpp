// SPDX-License-Identifier: GPL-2.0-or-later
#include "../callstage.h"
#include <QAbstractButton>
#include <QApplication>
#include <QSignalSpy>

static void check(bool ok, const char *message)
{
    if (!ok)
        qFatal("%s", message);
}

static QAbstractButton *previewOf(QWidget *content)
{
    return content->parentWidget()->findChild<QAbstractButton *>();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    CallStage    stage;
    auto         camera = new QWidget;
    auto         screen = new QWidget;
    auto         board  = new QWidget;
    stage.addTile(camera, QStringLiteral("Camera"), CallStage::CameraPriority);
    stage.addTile(screen, QStringLiteral("Screen"), CallStage::ScreenPriority);
    stage.addTile(board, QStringLiteral("Whiteboard"), CallStage::WhiteboardPriority);
    stage.resize(800, 500);
    stage.show();
    check(!stage.mainTile(), "an empty stage has a main tile");

    stage.setTileVisible(camera, true);
    check(stage.mainTile() == camera, "the only tile is not on the main stage");
    check(!previewOf(camera)->isVisible(), "the main tile is covered by a preview overlay");

    // Default order: screen, then whiteboard, then camera.
    stage.setTileVisible(board, true);
    check(stage.mainTile() == board, "the whiteboard does not take the stage from the camera");
    stage.setTileVisible(screen, true);
    check(stage.mainTile() == screen, "a shared screen does not take the stage");
    check(previewOf(camera)->isVisible() && previewOf(board)->isVisible(), "other tiles are not previews");

    // A tile picked by the viewer stays until it disappears.
    previewOf(camera)->click();
    check(stage.mainTile() == camera, "clicking a preview does not move it to the main stage");
    stage.setTileVisible(screen, false);
    stage.setTileVisible(screen, true);
    check(stage.mainTile() == camera, "a new tile replaced the viewer's choice");
    stage.setTileVisible(camera, false);
    check(stage.mainTile() == screen, "the default order is not restored after the chosen tile left");

    stage.setMainTile(camera);
    check(stage.mainTile() == screen, "a hidden tile was put on the main stage");

    QSignalSpy changed(&stage, &CallStage::mainTileChanged);
    stage.hideAllTiles();
    check(!stage.mainTile() && changed.count() == 1, "hiding all tiles did not clear the main stage");
    check(!camera->isVisible() && !screen->isVisible() && !board->isVisible(), "hidden tiles are still shown");

    qInfo("call stage tests passed");
    return 0;
}
