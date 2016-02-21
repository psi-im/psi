#ifndef GLOBALSTATUSMENU_H
#define GLOBALSTATUSMENU_H

#include "psicon.h"
#include "statusmenu.h"

class GlobalStatusMenu : public StatusMenu {
	Q_OBJECT
private:
	void addChoose();
	void addReconnect();

public:
	GlobalStatusMenu(QWidget* parent, PsiCon* _psi)
		: StatusMenu(parent, _psi) { };

	void fill();

public slots:
	void preventStateChange(bool checked);
};

#endif // GLOBALSTATUSMENU_H
