#ifndef PSITRAYICON_H
#define PSITRAYICON_H

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

class PsiIcon;
class QMenu;
class QPoint;

class PsiTrayIcon : public QObject {
    Q_OBJECT
public:
    PsiTrayIcon(const QString &tip, QMenu *popup, QObject *parent = nullptr);
    ~PsiTrayIcon();

    void setContextMenu(QMenu *);
    void setToolTip(const QString &);
    void setIcon(const PsiIcon *);
    void setBadge(int attentionCount, bool unreadMuc);

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
    bool    eventFilter(QObject *, QEvent *);

private:
    PsiIcon         *icon_;
    QPointer<PsiIcon> sourceIcon_;
    QSystemTrayIcon *trayicon_;
    int              attentionCount_ = 0;
    bool             unreadMuc_ = false;
};

#endif // PSITRAYICON_H
