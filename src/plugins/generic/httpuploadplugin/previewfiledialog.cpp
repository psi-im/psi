#include "previewfiledialog.h"
#include <QGridLayout>

PreviewFileDialog::PreviewFileDialog(QWidget* parent, const QString & caption, const QString & directory,
		const QString & filter, int previewWidth) :
		QFileDialog(parent, caption, directory, filter) {
	QGridLayout *layout = (QGridLayout*) this->layout();
	if (!layout) {
		// this QFileDialog is a native one (Windows/KDE/...) and doesn't need to be extended with preview
		return;
	}
	setObjectName("PreviewFileDialog");
	QVBoxLayout* box = new QVBoxLayout();

	mpPreview = new QLabel(tr("Preview"), this);
	mpPreview->setAlignment(Qt::AlignCenter);
	mpPreview->setObjectName("labelPreview");
	mpPreview->setMinimumWidth(previewWidth);
	mpPreview->setMinimumHeight(height());
	setMinimumWidth(width() + previewWidth);
	box->addWidget(mpPreview);

	box->addStretch();

	// add to QFileDialog layout
	layout->addLayout(box, 1, 3, 3, 1);
	connect(this, SIGNAL(currentChanged(const QString&)), this, SLOT(onCurrentChanged(const QString&)));
}

void PreviewFileDialog::onCurrentChanged(const QString & path) {
	QPixmap pixmap = QPixmap(path);
	if (pixmap.isNull()) {
		mpPreview->setText(tr("Not an image"));
	} else {
		mpPreview->setPixmap(
				pixmap.scaled(mpPreview->width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
}
