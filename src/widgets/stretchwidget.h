#ifndef STRETCHWIDGET_H
#define STRETCHWIDGET_H

class StretchWidget: public QWidget
{
public:
	StretchWidget(QWidget *parent) : QWidget( parent )
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}
};

#endif
