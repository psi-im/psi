#ifndef SNARLINTERFACE_H
#define SNARLINTERFACE_H

#include <string>
#include <windows.h>

class SnarlInterface {
public:
    static const unsigned int SNARL_STRING_LENGTH = 1024;

    static const int SNARL_LAUNCHED = 1;
    static const int SNARL_QUIT     = 2;

    static const int SNARL_NOTIFICATION_CLICKED   = 32;
    static const int SNARL_NOTIFICATION_TIMED_OUT = 33;
    static const int SNARL_NOTIFICATION_ACK       = 34;

    enum SNARL_COMMANDS {
        SNARL_SHOW = 1,
        SNARL_HIDE,
        SNARL_UPDATE,
        SNARL_IS_VISIBLE,
        SNARL_GET_VERSION,
        SNARL_REGISTER_CONFIG_WINDOW,
        SNARL_REVOKE_CONFIG_WINDOW
    };

    struct SNARLSTRUCT {
        SNARL_COMMANDS cmd      = SNARL_COMMANDS::SNARL_SHOW;
        long           id       = 0;
        long           timeout  = 0;
        long           lngData2 = 0;
        char           title[SNARL_STRING_LENGTH];
        char           text[SNARL_STRING_LENGTH];
        char           icon[SNARL_STRING_LENGTH];

        SNARLSTRUCT()
        {
            memset(title, 0, SNARL_STRING_LENGTH);
            memset(text, 0, SNARL_STRING_LENGTH);
            memset(icon, 0, SNARL_STRING_LENGTH);
        }
    };

    SnarlInterface();
    ~SnarlInterface();
    long snShowMessage(std::string title, std::string text, long timeout, std::string iconPath, HWND hWndReply,
                       long uReplyMsg);
    bool snHideMessage(long id);
    bool snIsMessageVisible(long id);
    bool snUpdateMessage(long id, std::string title, std::string text);
    bool snRegisterConfig(HWND hWnd, std::string appName, long replyMsg);
    bool snRevokeConfig(HWND hWnd);
    bool snGetVersion(int *major, int *minor);
    long snGetGlobalMsg();

private:
    std::string SNARL_GLOBAL_MESSAGE;

    long send(SNARLSTRUCT snarlStruct);
};

#endif // SNARLINTERFACE_H
