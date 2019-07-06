#ifndef OPT_APPEARANCEGENERAL_H
#define OPT_APPEARANCEGENERAL_H

#include "optionstab.h"

#include <QHash>
#include <QLineEdit>
#include <QAbstractButton>

class QCheckBox;

class FontLabel : public QLineEdit
{
    Q_OBJECT
public:
    FontLabel(QWidget *parent = nullptr, const char *name = nullptr);

    void setFont(QString);
    QString fontName() const;

    QSize sizeHint() const;

private:
    QString m_font;
    int m_defaultHeight;
};

class QWidget;
class QButtonGroup;
class QLineEdit;

class OptionsTabAppearance : public MetaOptionsTab
{
    Q_OBJECT
public:
    OptionsTabAppearance(QObject *parent);
    void setData(PsiCon *, QWidget *);
};

class OptionsTabIconset : public MetaOptionsTab
{
    Q_OBJECT
public:
    OptionsTabIconset(QObject *parent);
};

class OptionsTabAppearanceMisc : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabAppearanceMisc(QObject *parent);
    ~OptionsTabAppearanceMisc();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private slots:
    void setData(PsiCon *, QWidget *);

private:
    QWidget *w = nullptr, *parentWidget = nullptr;
};

class OptionsTabAppearanceGeneral : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabAppearanceGeneral(QObject *parent);
    ~OptionsTabAppearanceGeneral();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private slots:
    void setData(PsiCon *, QWidget *);
    void chooseColor(QAbstractButton* button);
    void colorCheckBoxClicked(int);
    void chooseFont(QAbstractButton* button);

private:
    QWidget *w = nullptr, *parentWidget = nullptr;
    QButtonGroup *bg_color = nullptr;
    FontLabel *le_font[4] = {nullptr, nullptr, nullptr, nullptr};
    QButtonGroup *bg_font = nullptr;

    typedef QHash<QCheckBox*, QPair<QAbstractButton*,QString> > ColorWidgetsMap;
    ColorWidgetsMap colorWidgetsMap;
};

#endif
