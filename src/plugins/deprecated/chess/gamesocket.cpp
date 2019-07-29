/*
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2005  SilverSoft.Net
 * All rights reserved
 *
 * $Id: gamesocket.cpp,v 0.1 2005/01/08 12:31:24 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#include "gamesocket.h"

#include <Q3Socket>

GameSocket::GameSocket(QWidget *parent, const char *name)
    :Q3ServerSocket(GAME_PORT, GAME_BACKLOG, (QObject *)parent, name)
{
}

GameSocket::~GameSocket()
{
}

void
GameSocket::newConnection(int sock)
{

    emit acceptConnection(sock);
}

