#include "filesharedlg.h"
#include "ui_filesharedlg.h"

FileShareDlg::FileShareDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileShareDlg)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);
    ui->pixmapRatioLabel->hide();
}

void FileShareDlg::setImage(const QImage &img)
{
    auto pix = QPixmap::fromImage(img);

    if (pix.size().boundedTo(QSize(500, 400)) != pix.size()) {
        pix.scaled(QSize(500, 400), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    ui->pixmapRatioLabel->setPixmap(QPixmap::fromImage(img));
    ui->pixmapRatioLabel->show();
}

QString FileShareDlg::description() const
{
    return ui->lineEdit->toPlainText();
}

FileShareDlg::~FileShareDlg()
{
    delete ui;
}
