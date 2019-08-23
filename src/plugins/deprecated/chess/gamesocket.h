/*
 * Copyright (C) 2005  SilverSoft.Net
 * All rights reserved
 *
 * $Id: gamesocket.h,v 0.1 2005/01/08 12:31:24 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#ifndef GAMESOCKET_H
#define GAMESOCKET_H

#include <Q3ServerSocket>
#include <stdlib.h>

#define GAME_PORT 1345
#define GAME_BACKLOG 5

class GameSocket:public Q3ServerSocket
{
    Q_OBJECT
public:
    GameSocket(QWidget *parent = NULL, const char *name = NULL);
    ~GameSocket();

private:

protected:
    void    newConnection(int);

signals:
    void    acceptConnection(int);
};

#endif // GAMESOCKET_H
