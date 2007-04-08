#ifndef QWEXTEND_H
#define QWEXTEND_H

#include <qglobal.h>
#include <Qt>

#ifdef Q_WS_X11
class QWidget;
void reparent_good(QWidget *that, Qt::WFlags f, bool showIt);
#endif

#endif
