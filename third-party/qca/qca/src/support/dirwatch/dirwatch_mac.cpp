/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "dirwatch_p.h"

namespace QCA {

DirWatchPlatform::DirWatchPlatform(QObject *parent) : QObject(parent) {}
DirWatchPlatform::~DirWatchPlatform() {}
bool DirWatchPlatform::isSupported() { return false; }
bool DirWatchPlatform::init() { return false; }
int DirWatchPlatform::addDir(const QString &) { return -1; }
void DirWatchPlatform::removeDir(int) {}

}
