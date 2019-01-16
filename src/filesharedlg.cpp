#include "filesharedlg.h"
#include "ui_filesharedlg.h"

#include <QApplication>
#include <QDesktopWidget>

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

    const auto sg = QApplication::desktop()->screenGeometry(this);
    if (pix.size().boundedTo(sg.size() / 2) != pix.size()) {
        pix.scaled(sg.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
