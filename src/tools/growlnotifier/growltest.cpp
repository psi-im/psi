/*
 * growltest.cpp: A test program for the GrowlNotifier class
 * Copyright (C) 2005  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "growlnotifier.h"

#include <QStringList>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPixmap>
#include <QGridLayout>

class GrowlTestWidget : public QWidget
{
	Q_OBJECT

public:
    GrowlTestWidget(QWidget *parent=0);

public slots:
	void do_notification1();
	void do_notification2();
	void notification_clicked();

private:
	QLineEdit *text, *title;
	QCheckBox* sticky;
	GrowlNotifier* growlNotifier;
};


GrowlTestWidget::GrowlTestWidget(QWidget *parent) : QWidget(parent)
{
	// Initialize widgets
	QGridLayout *layout = new QGridLayout(this);

	layout->addWidget(new QLabel("Title",this),0,0);
	title = new QLineEdit(this);
	title->setText("My Text");
	layout->addWidget(title,0,1);

	layout->addWidget(new QLabel("Text",this),1,0);
	text = new QLineEdit(this);
	text->setText("My Description");
	layout->addWidget(text,1,1);

	//layout->addWidget(new QLabel("Sticky",this),2,0);
	//sticky = new QCheckBox(this);
	//sticky->setTristate();
	//layout->addWidget(sticky,2,1);
	
    QPushButton *notification1 = new QPushButton( "Notification 1", this );
	connect(notification1, SIGNAL(clicked()), SLOT(do_notification1()));
	layout->addWidget(notification1,3,0);

	QPushButton *notification2 = new QPushButton( "Notification 2", this );
	connect(notification2, SIGNAL(clicked()), SLOT(do_notification2()));
	layout->addWidget(notification2,3,1);
	
	// Initialize GrowlNotifier
	QStringList nots, defaults;
	nots << "Notification 1" << "Notification 2";
	defaults << "Notification 1";
	growlNotifier = new GrowlNotifier(nots, defaults, "GrowlNotifierTest");
}

int main( int argc, char **argv )
{
    QApplication a( argc, argv );
    GrowlTestWidget w;
    w.show();
    return a.exec();
}
	
void GrowlTestWidget::do_notification1()
{
	//if (sticky->state() != QButton::NoChange) {
	//	growlNotifier->notify("Notification 1", title->text(), text->text(), QPixmap(), sticky->isChecked(), this, SLOT(notification_clicked()));
	//}
	//else {
	//	growlNotifier->notify("Notification 1", title->text(), text->text(), QPixmap());
	//}
	growlNotifier->notify("Notification 1", title->text(), text->text(), QPixmap(), false, this, SLOT(notification_clicked()));
}

void GrowlTestWidget::do_notification2()
{
	//if (sticky->state() != QButton::NoChange) {
	//	growlNotifier->notify("Notification 2", title->text(), text->text(), QPixmap(), sticky->isChecked(), this, SLOT(notification_clicked()));
	//}
	//else {
	//	growlNotifier->notify("Notification 2", title->text(), text->text(), QPixmap());
	//}
		growlNotifier->notify("Notification 2", title->text(), text->text(), QPixmap(), false, this, SLOT(notification_clicked()));
}

void GrowlTestWidget::notification_clicked()
{
	QMessageBox::information(0, "Information", "Notification was clicked\n");
}

#include "growltest.moc"
