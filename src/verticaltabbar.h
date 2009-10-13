#ifndef VERTICALTABBAR_H
#define VERTICALTABBAR_H

#include <QWidget>

class VerticalTabBar : public QWidget
{
	Q_OBJECT

public:
	enum IndicateMode
	{
		None,
		Busy,
		Attention
	};

	VerticalTabBar(QWidget *parent = 0);
	~VerticalTabBar();

	void addTab(int pos, const QString &text, IndicateMode mode = None);
	void updateTab(int pos, const QString &text, IndicateMode mode = None);
	void removeTab(int pos);

	void setSelected(int pos);

signals:
	void tabClicked(int num);
	void tabMenuActivated(int num, const QPoint &pos);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
