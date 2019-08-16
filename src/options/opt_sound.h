#ifndef OPT_SOUND_H
#define OPT_SOUND_H

#include "optionstab.h"

#include <QMap>

class QAbstractButton;
class QButtonGroup;
class QLineEdit;
class QWidget;

class OptionsTabSound : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabSound(QObject *parent);
    ~OptionsTabSound();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private slots:
    void chooseSoundEvent(QAbstractButton*);
    void previewSoundEvent(QAbstractButton*);
    void soundReset();
    void setData(PsiCon *, QWidget *);

private:
    QWidget *w = nullptr, *parentWidget = nullptr;
    QList<QLineEdit*> sounds_;
    QMap<QAbstractButton*,QLineEdit*> modify_buttons_;
    QMap<QAbstractButton*,QLineEdit*> play_buttons_;
    QButtonGroup *bg_se = nullptr, *bg_sePlay = nullptr;
};

#endif // OPT_SOUND_H
