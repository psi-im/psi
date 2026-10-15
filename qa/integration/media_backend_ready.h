// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "psimediaprovider.h"
#include <QEventLoop>
#include <QTimer>
#include <memory>

// Integration drivers may wait for the public feature lookup to complete.
// Provider initialization alone does not mean its asynchronous crypto probe is
// finished. An empty completed result is still ready; callers check capability.
inline bool waitForMediaBackend(PsiMedia::Provider &provider, int timeoutMs = 10000)
{
    std::unique_ptr<PsiMedia::FeaturesContext> features(provider.createFeatures());
    if (!features)
        return false;
    QEventLoop loop;
    QTimer     deadline;
    bool       ready = false;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(timeoutMs);
    features->lookup(PsiMedia::FeaturesContext::AudioModes | PsiMedia::FeaturesContext::VideoModes, &loop,
                     [&](const PsiMedia::PFeatures &) {
                         ready = true;
                         loop.quit();
                     });
    if (!ready)
        loop.exec();
    return ready;
}
