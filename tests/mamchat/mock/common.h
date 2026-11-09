#pragma once
#include <QtGlobal>
// The application-wide DPI helper is the only common.h dependency of PsiRichText.
inline int pointToPixel(qreal point) { return qRound(point * 96.0 / 72.0); }

// TabBar's pinned-tab artwork scales with the same text/icon ratio as Psi.
constexpr double EqTextIconK = 0.93;
