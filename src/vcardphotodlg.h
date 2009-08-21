#ifndef VCARDPHOTODLG_H
#define VCARDPHOTODLG_H

#include <QLabel>
#include <QDialog>
#include <QToolBar>

class ShowPhotoDlg : public QDialog
{
	Q_OBJECT
public:
	ShowPhotoDlg(QWidget *parent, QPixmap &pixmap);
private:
	QDialog *showPhotoDlg;
	QLabel *label;
	QPixmap photoPixmap;
	QToolBar *toolbar;
	QAction *saveAct;
	QAction *restoreAct;
	bool initSize;
	void createActions();
	void updatePhoto(const QSize size);
protected:
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent * event);
private slots:
	void save();
	void restore();
};

#endif
