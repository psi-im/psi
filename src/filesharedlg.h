#ifndef FILESHAREDLG_H
#define FILESHAREDLG_H

#include <QDialog>

namespace Ui {
class FileShareDlg;
}

class FileShareDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FileShareDlg(QWidget *parent = nullptr);
    ~FileShareDlg();

    void setImage(const QImage &img);
    QString description() const;
private:
    Ui::FileShareDlg *ui;
};

#endif // FILESHAREDLG_H
