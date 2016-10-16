#ifndef PREVIEW_FILE_DIALOG_H
#define PREVIEW_FILE_DIALOG_H

#include <QFileDialog>
#include <QLabel>
// based on http://www.qtcentre.org/threads/33593-How-can-i-have-a-QFileDialog-with-a-preview-of-the-picture
class PreviewFileDialog: public QFileDialog {
Q_OBJECT
public:
	explicit PreviewFileDialog(QWidget* parent = 0, const QString & caption = QString(), const QString & directory =
			QString(), const QString & filter = QString(), int previewWidth = 150);

private slots:
	void onCurrentChanged(const QString & path);

protected:
	QLabel* mpPreview;

};

#endif // PREVIEW_FILE_DIALOG_H
