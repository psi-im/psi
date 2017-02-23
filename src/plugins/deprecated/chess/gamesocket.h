/*
 * Copyright (c) 2005 by SilverSoft.Net
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

#ifndef	__GAME_SOCKET_H__
#define	__GAME_SOCKET_H__

#include <Q3ServerSocket>
#include <stdlib.h>

#define	GAME_PORT	1345
#define	GAME_BACKLOG	5

class GameSocket:public Q3ServerSocket
{
	Q_OBJECT
public:
	GameSocket(QWidget *parent = NULL, const char *name = NULL);
	~GameSocket();

private:

protected:
	void	newConnection(int);

signals:
	void	acceptConnection(int);
};

#endif	/* __GAME_SOCKET_H__ */
