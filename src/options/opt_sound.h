#ifndef OPT_SOUND_H
#define OPT_SOUND_H

#include <QMap>

#include "optionstab.h"

class QWidget;
class QLineEdit;
class QButtonGroup;
class QAbstractButton;

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
	QWidget *w, *parentWidget;
	QList<QLineEdit*> sounds_;
	QMap<QAbstractButton*,QLineEdit*> modify_buttons_;
	QMap<QAbstractButton*,QLineEdit*> play_buttons_;
	QButtonGroup *bg_se, *bg_sePlay;
};

#endif
