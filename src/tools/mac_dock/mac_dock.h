#ifndef MAC_DOCK_H
#define MAC_DOCK_H

#include <QString>

class MacDock {
public:
    static void startBounce();
    static void stopBounce();
    static void overlay(const QString &text = QString());

private:
    static bool isBouncing;
    static bool overlayed;
};

#endif // MAC_DOCK_H
