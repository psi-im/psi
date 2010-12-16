/*
 * Copyright (c) 2005 by SilverSoft.Net
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

#include <Q3Socket>

#include "gamesocket.h"

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

