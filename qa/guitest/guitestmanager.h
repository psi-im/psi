#ifndef GUITESTMANAGER_H
#define GUITESTMANAGER_H

#include "guitest.h"

#include <QStringList>

class GUITestManager
{
public:
    static GUITestManager* instance();

    void registerTest(GUITest* test);
    bool runTest(const QString& name);
    QStringList getTestNames() const;

private:
    GUITestManager();

    static GUITestManager* instance_;
    QList<GUITest*> tests_;
};

#endif
