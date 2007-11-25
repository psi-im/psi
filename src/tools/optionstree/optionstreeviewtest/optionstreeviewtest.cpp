#include <QApplication>
#include <QTreeView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QCheckBox>

#include "optionstree.h"
#include "optionstreemodel.h"

class MyHeaderViewWidget : public QWidget 
{
	Q_OBJECT

public:
	MyHeaderViewWidget(QWidget* parent = 0) : QWidget(parent) {
		o_.loadOptions("../options.xml","OptionsTest", "", "0.1");
		tm_ = new OptionsTreeModel(&o_);
		
		QVBoxLayout* layout = new QVBoxLayout(this);
		
		tv_ = new QTreeView(this);
		tv_->setModel(tm_);
		tv_->setAlternatingRowColors(true);
		layout->addWidget(tv_);
		
		cb_ = new QCheckBox(this);
		cb_->setText(tr("Flat"));
		connect(cb_,SIGNAL(toggled(bool)),tm_,SLOT(setFlat(bool)));
		layout->addWidget(cb_);
	}
	~MyHeaderViewWidget() {
		o_.saveOptions("../options.xml","OptionsTest", "", "0.1");
	}
private:
	OptionsTree o_;
	QTreeView* tv_;
	OptionsTreeModel* tm_;
	QCheckBox* cb_;
};

int main(int argc, char* argv[])
{
	QApplication app(argc,argv);
	MyHeaderViewWidget w;
	w.resize(600,300);
	w.show();
	return app.exec();
}
