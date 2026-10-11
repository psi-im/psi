// SPDX-License-Identifier: GPL-2.0-or-later
// The visual area of a call: one tile on the main stage, the other visible tiles as previews.
#ifndef CALLSTAGE_H
#define CALLSTAGE_H

#include <QFrame>
#include <QList>
#include <QPointer>

class QBoxLayout;
class QLabel;

class CallStage : public QFrame {
    Q_OBJECT
public:
    // Default preference for the main stage; a higher value wins.
    enum Priority { CameraPriority = 0, WhiteboardPriority = 1, ScreenPriority = 2 };

    explicit CallStage(QWidget *parent = nullptr);

    // Adds a hidden tile. The stage takes ownership of the content widget.
    void     addTile(QWidget *content, const QString &title, int priority);
    void     setTileVisible(QWidget *content, bool visible);
    bool     isTileVisible(QWidget *content) const;
    void     hideAllTiles();
    QWidget *mainTile() const;
    // Puts a visible tile on the main stage until it is hidden, as a click on its preview does.
    void setMainTile(QWidget *content);
    void setPlaceholderText(const QString &text);

signals:
    void mainTileChanged(QWidget *content);

private:
    class TileFrame;

    TileFrame *frameOf(QWidget *content) const;
    void       updateLayout();

    QList<TileFrame *>  tiles_;
    QPointer<QWidget>   pinned_;
    QPointer<QWidget>   main_;
    QLabel             *placeholder_;
    QBoxLayout         *mainLayout_;
    QBoxLayout         *sideLayout_;
    QWidget            *side_;
};

#endif // CALLSTAGE_H
