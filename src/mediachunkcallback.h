#pragma once

#include "mediachunktypes.h"
#include <QByteArray>
#include <QObject>
#include <functional>

// Thread-safe provider completion delivery. Qt disconnects the receiver on
// destruction; worker callbacks never dereference a possibly deleted QPointer.
std::function<void(QByteArray, MediaChunkError)>
queuedMediaChunkCompletion(QObject *receiver, std::function<void(QByteArray, MediaChunkError)> done);
