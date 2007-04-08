#ifndef UPDATINGCOMBOBOX_H
#define UPDATINGCOMBOBOX_H

#include <QComboBox>

class UpdatingComboBox : public QComboBox
{
	Q_OBJECT

public:
	UpdatingComboBox(QWidget* parent) : QComboBox(parent) {}

	virtual void showPopup() { 
		emit popup();
		QComboBox::showPopup();
	}
	
signals:
	void popup();
};

#endif
