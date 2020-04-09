#ifndef PSITIPLABEL_H
#define PSITIPLABEL_H

#include <QBasicTimer>
#include <QFrame>

class QTextDocument;

class PsiTipLabel : public QFrame {
    Q_OBJECT
public:
    PsiTipLabel(QWidget *parent);
    ~PsiTipLabel();

    virtual void        init(const QString &text);
    static PsiTipLabel *instance();

    void setText(const QString &text);

    // int heightForWidth(int w) const;
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    bool  eventFilter(QObject *, QEvent *);

    QString theText() const;

    QBasicTimer hideTimer, deleteTimer;

    void hideTip();

protected:
    void enterEvent(QEvent *);
    void timerEvent(QTimerEvent *e);
    void paintEvent(QPaintEvent *e);
    // QSize sizeForWidth(int w) const;

private:
    QTextDocument *doc;
    QString        theText_;
    bool           isRichText;
    int            margin;
    bool           enableColoring_;
    // int indent;

    virtual void initUi();
    virtual void startHideTimer();

    static PsiTipLabel *instance_;
};

#endif // PSITIPLABEL_H
