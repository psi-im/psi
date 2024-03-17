/*
 * privateqt_mac.mm
 * Copyright (C) 2009  Yandex LLC (Michail Pishchagin)
 * based on http://doc.trolltech.com/solutions/4/qtspellcheckingtextedit/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "privateqt_mac.h"

#import <AppKit/AppKit.h>
#include <QtCore/QVarLengthArray>
#include <QtDebug>

QString QtCFString::toQString(CFStringRef str)
{
    if (!str)
        return QString();
    CFIndex        length = CFStringGetLength(str);
    const UniChar *chars  = CFStringGetCharactersPtr(str);
    if (chars)
        return QString(reinterpret_cast<const QChar *>(chars), length);

    QVarLengthArray<UniChar> buffer(length);
    CFStringGetCharacters(str, CFRangeMake(0, length), buffer.data());
    return QString(reinterpret_cast<const QChar *>(buffer.constData()), length);
}

QtCFString::operator QString() const
{
    if (string.isEmpty() && type)
        const_cast<QtCFString *>(this)->string = toQString(type);
    return string;
}

CFStringRef QtCFString::toCFStringRef(const QString &string)
{
    return CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(string.unicode()), string.length());
}

QtCFString::operator CFStringRef() const
{
    if (!type)
        const_cast<QtCFString *>(this)->type = toCFStringRef(string);
    return type;
}

QtMacCocoaAutoReleasePool::QtMacCocoaAutoReleasePool()
{
    NSApplicationLoad();
    pool = (void *)[[NSAutoreleasePool alloc] init];
}

QtMacCocoaAutoReleasePool::~QtMacCocoaAutoReleasePool() { [(NSAutoreleasePool *)pool release]; }
