#ifndef COMMANDEXECUTOR_H
#define COMMANDEXECUTOR_H

#include <QtPlugin>

/**
 * Generic API for arbitrary inter-plugin communication
 */
class CommandExecutor {
public:
    virtual ~CommandExecutor() { }

    virtual bool execute(int account, const QHash<QString, QVariant> &args, QHash<QString, QVariant> *result = nullptr)
        = 0;
};

Q_DECLARE_INTERFACE(CommandExecutor, "org.psi-im.CommandExecutor/0.1");

#endif // COMMANDEXECUTOR_H
