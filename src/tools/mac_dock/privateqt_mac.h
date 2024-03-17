/*
 * privateqt_mac.h
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

#ifndef PRIVATEQT_MAC_H
#define PRIVATEQT_MAC_H

#include <CoreFoundation/CoreFoundation.h>

#include <QtCore/QString>

template <typename T> class QtCFType {
public:
    inline QtCFType(const T &t = 0) : type(t) { }
    inline QtCFType(const QtCFType &helper) : type(helper.type)
    {
        if (type)
            CFRetain(type);
    }
    inline ~QtCFType()
    {
        if (type)
            CFRelease(type);
    }
    inline operator T() { return type; }
    inline QtCFType operator=(const QtCFType &helper)
    {
        if (helper.type)
            CFRetain(helper.type);
        CFTypeRef type2 = type;
        type            = helper.type;
        if (type2)
            CFRelease(type2);
        return *this;
    }
    inline T       *operator&() { return &type; }
    static QtCFType constructFromGet(const T &t)
    {
        CFRetain(t);
        return QtCFType<T>(t);
    }

protected:
    T type;
};

class QtCFString : public QtCFType<CFStringRef> {
public:
    inline QtCFString(const QString &str) : QtCFType<CFStringRef>(0), string(str) { }
    inline QtCFString(const CFStringRef cfstr = 0) : QtCFType<CFStringRef>(cfstr) { }
    inline QtCFString(const QtCFType<CFStringRef> &other) : QtCFType<CFStringRef>(other) { }
    operator QString() const;
    operator CFStringRef() const;
    static QString     toQString(CFStringRef cfstr);
    static CFStringRef toCFStringRef(const QString &str);

private:
    QString string;
};

class QtMacCocoaAutoReleasePool {
private:
    void *pool;

public:
    QtMacCocoaAutoReleasePool();
    ~QtMacCocoaAutoReleasePool();

    inline void *handle() const { return pool; }
};

struct _NSRange;
typedef struct _NSRange NSRange;

#endif // PRIVATEQT_MAC_H
