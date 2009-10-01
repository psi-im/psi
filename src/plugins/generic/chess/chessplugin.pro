include(../../psiplugin.pri)

# Input
HEADERS += gameboard.h \
           gamesocket.h \
           mainwindow.h \
           xpm/black_bishop.xpm \
           xpm/black_castle.xpm \
           xpm/black_king.xpm \
           xpm/black_knight.xpm \
           xpm/black_pawn.xpm \
           xpm/black_queen.xpm \
           xpm/white_bishop.xpm \
           xpm/white_castle.xpm \
           xpm/white_king.xpm \
           xpm/white_knight.xpm \
           xpm/white_pawn.xpm \
           xpm/white_queen.xpm \
           xpm/chess.xpm \
           xpm/quit.xpm \
           xpm/new_game.xpm

SOURCES += gameboard.cpp gamesocket.cpp mainwindow.cpp

SOURCES += chessplugin.cpp

