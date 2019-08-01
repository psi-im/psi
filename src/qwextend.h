#ifndef QWEXTEND_H
#define QWEXTEND_H

#include <Qt>
#include <qglobal.h>

#ifdef HAVE_X11
    class QWidget;
    void reparent_good(QWidget *that, Qt::WindowFlags f, bool showIt);
#endif

#endif // QWEXTEND_H
