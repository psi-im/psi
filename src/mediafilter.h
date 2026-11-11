/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

// Shared gallery selection policy; no database, protocol or widget dependency.
struct MediaFilter {
    enum Type { Images = 1, Videos = 2, Files = 4 };
    enum Availability { Remote = 1, Partial = 2, Complete = 4 };
    int  types        = Images | Videos | Files;
    int  availability = Remote | Partial | Complete;
    bool operator==(const MediaFilter &other) const
    {
        return types == other.types && availability == other.availability;
    }
};
