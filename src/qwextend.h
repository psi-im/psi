#ifndef QWEXTEND_H
#define QWEXTEND_H

#include <qglobal.h>
#include <Qt>

#ifdef HAVE_X11
class QWidget;
void reparent_good(QWidget *that, Qt::WindowFlags f, bool showIt);
#endif

#endif
