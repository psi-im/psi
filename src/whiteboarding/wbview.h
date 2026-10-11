/*
 * wbview.h - whiteboard canvas with its toolbar
 * Copyright (C) 2006  Joonas Govenius
 *
 * Originally developed from:
 * wbdlg.h - dialog for handling chats
 * Copyright (C) 2001-2002  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WBVIEW_H
#define WBVIEW_H

#include "wbwidget.h"

#include <QPointer>
#include <QWidget>

class IconAction;
class QActionGroup;
class QDialog;
class QMenu;
class QToolBar;

/*! \brief A whiteboard: the WbWidget canvas with its tools.
 *
 *  Shows a shared document and provides the tools, stroke and fill settings,
 *  saving, importing and the controls help. The call window shows it as a tile;
 *  as a preview the toolbar is hidden and the canvas is read-only.
 *
 *  \sa WbWidget
 */
class WbView : public QWidget {
    Q_OBJECT

public:
    /*! \brief Creates a view of \a document. The document is not owned.*/
    explicit WbView(XMPP::Jingle::SXE::Document *document, QWidget *parent = nullptr);

    XMPP::Jingle::SXE::Document *document() const;
    /*! \brief Allows or forbids changing the document; the tools follow.*/
    void setEditable(bool editable);
    bool isEditable() const;
    /*! \brief Shows the canvas as a fitted, read-only preview without the toolbar.*/
    void setPreview(bool preview);

public slots:
    /*! \brief Asks for a file name and saves the document as SVG.*/
    void save();
    /*! \brief Asks for an SVG file and adds its elements to the whiteboard.*/
    void importSvg();
    /*! \brief Shows the mouse and keyboard controls.*/
    void showHelp();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setStrokeColor();
    void setFillColor(bool fill);
    void setCanvasSize();
    void buildMenu();
    void updateTools();

    WbWidget                              *wbWidget_;
    QToolBar                              *toolbar_;
    QMenu                                 *menu_;
    QActionGroup                          *group_widths_;
    QActionGroup                          *group_modes_;
    QAction                               *act_color_, *act_fill_, *act_help_;
    IconAction                            *act_clear_, *act_save_, *act_import_, *act_geometry_, *act_widths_,
        *act_modes_;
    QPointer<QDialog>                      helpDialog_;
    QPointer<XMPP::Jingle::SXE::Document> document_;
    bool                                   editable_ = true;
    bool                                   preview_  = false;
};

#endif // WBVIEW_H
