#include "SnarlInterface.h"
#include <QtCore>
#include <stdlib.h>
#include <string>

using namespace std;


SnarlInterface::SnarlInterface() {
	SNARL_GLOBAL_MESSAGE = "SnarlGlobalMessage";
}

SnarlInterface::~SnarlInterface() {

}

long SnarlInterface::snShowMessage(std::string title, std::string text, long timeout, std::string iconPath, HWND hWndReply, long uReplyMsg) {
    SNARLSTRUCT snarlStruct;
    snarlStruct.cmd = SNARL_SHOW;
    if (title.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.title, (title.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.title, title.c_str());
    }
    if (text.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.text, (text.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.text, text.c_str());
    }
    if (iconPath.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.icon, (iconPath.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.icon, iconPath.c_str());
    }
    snarlStruct.timeout = timeout;
    snarlStruct.lngData2 = reinterpret_cast<long>(hWndReply);
    snarlStruct.id = uReplyMsg;
    return send(snarlStruct);
}

bool SnarlInterface::snHideMessage(long id) {
    SNARLSTRUCT snarlStruct;
    snarlStruct.id = id;
    snarlStruct.cmd = SNARL_HIDE;
    return static_cast<bool>(send(snarlStruct));
}

bool SnarlInterface::snIsMessageVisible(long id) {
    SNARLSTRUCT snarlStruct;
    snarlStruct.id = id;
    snarlStruct.cmd = SNARL_IS_VISIBLE;
    return static_cast<bool>(send(snarlStruct));
}

bool SnarlInterface::snUpdateMessage(long id, std::string title, std::string text) {
	SNARLSTRUCT snarlStruct;
    snarlStruct.id = id;
    snarlStruct.cmd = SNARL_UPDATE;
    if (title.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.title, (title.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.title, title.c_str());
    }
    if (text.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.text, (text.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.text, text.c_str());
    }
    return static_cast<bool>(send(snarlStruct));
}

bool SnarlInterface::snRegisterConfig(HWND hWnd, std::string appName, long replyMsg) {
    SNARLSTRUCT snarlStruct;
    snarlStruct.cmd = SNARL_REGISTER_CONFIG_WINDOW;
    snarlStruct.lngData2 = reinterpret_cast<long>(hWnd);
    snarlStruct.id = replyMsg;
    if (appName.length() > SNARL_STRING_LENGTH) {
        strcpy(snarlStruct.title, (appName.substr(0, SNARL_STRING_LENGTH-1)).c_str());
    }
    else {
        strcpy(snarlStruct.title, appName.c_str());
    }
    return static_cast<bool>(send(snarlStruct));
}

bool SnarlInterface::snRevokeConfig(HWND hWnd) {
	SNARLSTRUCT snarlStruct;
    snarlStruct.cmd = SNARL_REVOKE_CONFIG_WINDOW;
    snarlStruct.lngData2 = reinterpret_cast<long>(hWnd);
    return static_cast<bool>(send(snarlStruct));
}

bool SnarlInterface::snGetVersion(int* major, int* minor) {
	SNARLSTRUCT snarlStruct;
    snarlStruct.cmd = SNARL_GET_VERSION;
    long versionInfo = send(snarlStruct);
	if (versionInfo) {
		int maj = static_cast<int>(HIWORD(versionInfo));
		*major = maj;
		int min = static_cast<int>(LOWORD(versionInfo));
		*minor = min;
        return true;
	}
	return false;
}

long SnarlInterface::snGetGlobalMsg() {
	std::wstring tmp;
	tmp= QString(SNARL_GLOBAL_MESSAGE.c_str()).toStdWString();
	return RegisterWindowMessage(tmp.c_str());
}

long SnarlInterface::send(SNARLSTRUCT snarlStruct) {
	HWND hWnd = FindWindow(NULL, QString("Snarl").toStdWString().c_str());
	if (IsWindow(hWnd)) {
		COPYDATASTRUCT cds;
		cds.dwData = 2;
		cds.cbData = sizeof(snarlStruct);
		cds.lpData = &snarlStruct;
		LRESULT lr = SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&cds);
		if (lr) {
            return lr;
		}
	}
	return 0;
}
