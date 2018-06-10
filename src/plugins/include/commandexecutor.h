#ifndef PSI_COMMANDEXECUTOR_H
#define PSI_COMMANDEXECUTOR_H

/**
 * Generic API for arbitrary inter-plugin communication
 */
class CommandExecutor
{
public:
    virtual ~CommandExecutor() {}

    virtual bool execute(int account, const QHash<QString, QVariant> &args, QHash<QString, QVariant> *result = nullptr) = 0;
};

Q_DECLARE_INTERFACE(CommandExecutor, "org.psi-im.CommandExecutor/0.1");

#endif //PSI_COMMANDEXECUTOR_H
