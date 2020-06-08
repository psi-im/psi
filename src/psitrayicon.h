#ifndef PSITRAYICON_H
#define PSITRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>

class PsiIcon;
class QMenu;
class QPixmap;
class QPoint;

class PsiTrayIcon : public QObject {
    Q_OBJECT
public:
    PsiTrayIcon(const QString &tip, QMenu *popup, QObject *parent = nullptr);
    ~PsiTrayIcon();

    void setContextMenu(QMenu *);
    void setToolTip(const QString &);
    void setIcon(const PsiIcon *, bool alert = false);
    void setAlert(const PsiIcon *);

signals:
    void clicked(const QPoint &, int);
    void doubleClicked(const QPoint &);
    void closed();
    void doToolTip(QObject *, QPoint);

public slots:
    void show();
    void hide();

private slots:
    void animate();
    void trayicon_activated(QSystemTrayIcon::ActivationReason);

protected:
    QPixmap makeIcon();
    bool    eventFilter(QObject *, QEvent *);

private:
    PsiIcon *        icon_;
    QSystemTrayIcon *trayicon_;
    quintptr         realIcon_;
};

#endif // PSITRAYICON_H
