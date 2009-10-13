#include "verticaltabbar.h"

#include "cudaskin.h"

class VerticalTabBarLabel : public QLabel
{
	Q_OBJECT

public:
	VerticalTabBarLabel(QWidget *parent = 0) :
		QLabel(parent)
	{
	}

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent *event)
	{
		if(event->button() == Qt::LeftButton)
			emit clicked();
	}

	void mouseReleaseEvent(QMouseEvent *event)
	{
		Q_UNUSED(event);
	}
};

class VerticalTabBar::Private : public QObject
{
	Q_OBJECT

public:
	VerticalTabBar *q;

	class TabItem
	{
	public:
		VerticalTabBarLabel *label;
		QString text;
		VerticalTabBar::IndicateMode mode;
	};
	QList<TabItem> tabs;

	QVBoxLayout *layout;
	int selected;

	Private(VerticalTabBar *_q) :
		QObject(_q),
		q(_q),
		selected(-1)
	{
	}

	QSize idealSize() const
	{
		int wmax = 0;
		int hmax = 0;

		foreach(const TabItem &i, tabs)
		{
			QSize hint = cuda_verticaltabbar_sizehint(i.text, q);
			wmax = qMax(wmax, hint.width());
			hmax += hint.height();
		}

		return QSize(wmax, hmax);
	}

	void updateTabs()
	{
		// no larger than 200 pixels wide
		int widest_tab = qMin(idealSize().width(), 200);

		for(int n = 0; n < tabs.count(); ++n)
		{
			const TabItem &i = tabs[n];
			i.label->setPixmap(cuda_verticaltabbar_render(i.text, selected == n, i.mode, widest_tab, q));
		}
	}

private slots:
	void lb_clicked()
	{
		VerticalTabBarLabel *lb = (VerticalTabBarLabel *)sender();

		int at = -1;
		for(int n = 0; n < tabs.count(); ++n)
		{
			if(tabs[n].label == lb)
			{
				at = n;
				break;
			}
		}
		Q_ASSERT(at != -1);

		selected = at;
		updateTabs();

		//printf("tabClicked(%d)\n", at);
		emit q->tabClicked(at);
	}

	void lb_customContextMenuRequested(const QPoint &pos)
	{
		VerticalTabBarLabel *lb = (VerticalTabBarLabel *)sender();

		int at = -1;
		for(int n = 0; n < tabs.count(); ++n)
		{
			if(tabs[n].label == lb)
			{
				at = n;
				break;
			}
		}
		Q_ASSERT(at != -1);

		QPoint ppos = lb->mapToParent(pos);
		//printf("tabMenuActivated(%d, [%d,%d])\n", at, ppos.x(), ppos.y());
		emit q->tabMenuActivated(at, ppos);
	}
};

VerticalTabBar::VerticalTabBar(QWidget *parent) :
	QWidget(parent)
{
	d = new Private(this);

	d->layout = new QVBoxLayout(this);
	d->layout->setContentsMargins(0, 0, 0, 0);
	d->layout->setSpacing(0);
	d->layout->addStretch();

	cuda_applyTheme(this);
}

VerticalTabBar::~VerticalTabBar()
{
	delete d;
}

void VerticalTabBar::addTab(int pos, const QString &text, IndicateMode mode)
{
	Private::TabItem i;
	i.text = text;
	i.mode = mode;
	i.label = new VerticalTabBarLabel(this);
	i.label->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(i.label, SIGNAL(clicked()), d, SLOT(lb_clicked()));
	connect(i.label, SIGNAL(customContextMenuRequested(const QPoint &)), d, SLOT(lb_customContextMenuRequested(const QPoint &)));
	d->tabs.insert(pos, i);
	d->layout->insertWidget(pos, i.label);
	if(d->selected == -1)
		d->selected = 0;
	d->updateTabs();
}

void VerticalTabBar::updateTab(int pos, const QString &text, IndicateMode mode)
{
	d->tabs[pos].text = text;
	d->tabs[pos].mode = mode;
	d->updateTabs();
}

void VerticalTabBar::removeTab(int pos)
{
	//printf("removeTab(%d)\n", pos);
	Q_ASSERT(pos >= 0);

	// removing the selected item?
	/*if(d->selected == pos)
	{
		// if it's the last item, move down or invalidate
		if(d->tabs.count() == pos - 1)
		{
			if(pos > 0)
				d->selected = pos - 1;
			else
				d->selected = -1;
		}

		// if it's not the last item, then we keep the selection right
		//   where it is, which means the item after it ends up being
		//   selected
	}
	else
	{
		if(d->selected > pos)
			d->selected = pos - 1;
	}*/

	QLabel *i = d->tabs[pos].label;
	d->tabs.removeAt(pos);
	delete i;
	d->updateTabs();
}

void VerticalTabBar::setSelected(int pos)
{
	//printf("setSelected(%d)\n", pos);
	d->selected = pos;
	d->updateTabs();
}

#include "verticaltabbar.moc"
