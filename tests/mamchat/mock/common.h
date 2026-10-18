#pragma once
#include <QtGlobal>
// The application-wide DPI helper is the only common.h dependency of PsiRichText.
inline int pointToPixel(qreal point) { return qRound(point * 96.0 / 72.0); }
