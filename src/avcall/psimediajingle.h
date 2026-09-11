/*
 * Copyright (C) 2026  Psi Project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PSIMEDIAJINGLE_H
#define PSIMEDIAJINGLE_H

#include <memory>

namespace XMPP::Jingle::RTP {
class MediaProvider;
}

// Media adapter for the experimental Iris-native Jingle RTP path. Creating the
// provider has no signaling, device or capture side effects.
std::shared_ptr<XMPP::Jingle::RTP::MediaProvider> makePsiMediaJingleProvider();

#endif // PSIMEDIAJINGLE_H
