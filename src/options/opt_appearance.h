#ifndef OPT_APPEARANCEGENERAL_H
#define OPT_APPEARANCEGENERAL_H

#include "optionstab.h"

#include <qlineedit.h>
#include <qabstractbutton.h>

class FontLabel : public QLineEdit
{
	Q_OBJECT
public:
	FontLabel(QWidget *parent = 0, const char *name = 0);

	void setFont(QString);
	QString fontName() const;

	QSize sizeHint() const;

private:
	QString m_font;
	int m_defaultHeight;
};

class QWidget;
struct Options;
class QButtonGroup;
class QLineEdit;

class OptionsTabAppearance : public MetaOptionsTab
{
	Q_OBJECT
public:
	OptionsTabAppearance(QObject *parent);
};

class OptionsTabAppearanceMisc : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabAppearanceMisc(QObject *parent);
	~OptionsTabAppearanceMisc();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private slots:
	void setData(PsiCon *, QWidget *);

private:
	QWidget *w, *parentWidget;
	Options *o;
};

class OptionsTabAppearanceGeneral : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabAppearanceGeneral(QObject *parent);
	~OptionsTabAppearanceGeneral();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

private slots:
	void setData(PsiCon *, QWidget *);
	void chooseColor(QAbstractButton* button);
	void chooseFont(QAbstractButton* button);

private:
	QWidget *w, *parentWidget;
	QButtonGroup *bg_color;
	FontLabel *le_font[4];
	QButtonGroup *bg_font;
	Options *o;
};

#endif
