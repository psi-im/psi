#include "vcardphotodlg.h"

#include <QMouseEvent>
#include <QDialog>
#include <QVBoxLayout>
#include <QAction>
#include <QFileDialog>

#include "psiiconset.h"
#include "fileutil.h"

ShowPhotoDlg::ShowPhotoDlg(QWidget *parent, QPixmap &pixmap)
	: QDialog(parent), initSize(true)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	photoPixmap = pixmap;
	setWindowTitle(QString(tr("Photo Preview: %1")).arg(parent->windowTitle()));

	label=new QLabel(this);
	label->setAlignment(Qt::AlignCenter);
	label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	label->setContentsMargins(0, 0, 0, 0);
	QVBoxLayout* layout = new QVBoxLayout();
	toolbar = new QToolBar(this);
	toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->addWidget(toolbar);
	layout->addWidget(label);

	setLayout(layout);
	createActions();
	toolbar->addAction(restoreAct);
	toolbar->addAction(saveAct);

	restore();
}

void ShowPhotoDlg::createActions()
{
	 saveAct = new QAction(IconsetFactory::icon("psi/save").icon(), tr("&Save As..."), this);
	 connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

	 restoreAct = new QAction(IconsetFactory::icon("psi/reload").icon(), tr("&Restore Size"), this);
	 connect(restoreAct, SIGNAL(triggered()), this, SLOT(restore()));
}

void ShowPhotoDlg::save()
{
	QString fileName = FileUtil::getSaveFileName(this, tr("Save As"), tr("photo.png"),
		tr("PNG File (*.png);;JPEG File (*.jpeg);;BMP File (*.bmp);;PPM File (*.ppm);;All Files (*)"));
	if (!fileName.isEmpty()) {
		photoPixmap.save(fileName);
	}
}

void ShowPhotoDlg::restore()
{
	resize(photoPixmap.width(), photoPixmap.height() + toolbar->height());
}

void ShowPhotoDlg::updatePhoto(const QSize size)
{
	label->setPixmap(photoPixmap.scaled(
			size,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation
	));
}

void ShowPhotoDlg::resizeEvent(QResizeEvent *)
{
	if (initSize) {
		restore();
		label->resize(photoPixmap.size()); //small hack to resize properly on init.
		initSize = false;
	}
	updatePhoto(label->size());
}

void ShowPhotoDlg::wheelEvent(QWheelEvent * event)
{
	int delta = event->delta() / 8;
	int width, height;
	QSize ps = label->pixmap()->size();
	ps.scale(ps.width() + delta, ps.height() + delta, Qt::KeepAspectRatio);
	width = ps.width();
	height = ps.height() + toolbar->height();
	if ((event->x() < width) && (event->y() < height)) {
		resize(ps.width(), ps.height() + toolbar->height());
	}
}
