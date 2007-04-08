//Kevin Smith 2005

/* This file is heavily based upon part of the KDE libraries
    Copyright (C) 2003 Stephan Binner <binner@kde.org>
    Copyright (C) 2003 Zack Rusin <zack@kde.org>
*/

#ifndef PSITABWIDGET_H
#define PSITABWIDGET_H

#include <QTabWidget>
#include <QIconSet>

/**
 * \brief A widget containing multiple tabs
 *
 * @since 0.10
 */
class PsiTabWidget : public QTabWidget
{
    Q_OBJECT

public:
	/**
	 * Constructor
	 */
    PsiTabWidget( QWidget *parent = 0 );
    /**
     * Standard destructor.
     */
    virtual ~PsiTabWidget();
    /**
      Set the tab of the given widget to \a color.
    */

    void setTabColor( QWidget *, const QColor& color );
    QColor tabColor( QWidget * ) const;
#ifdef NO
    /**
      Returns true if the close button is shown on tabs
      when mouse is hovering over them.
    */
    bool hoverCloseButton() const;

	/**
	 * Sets the close icon used on the tabbar
	*/
	void setCloseIcon(const QIconSet&);
		
public slots:
    /**
      If \a enable is true, a close button will be shown on mouse hover
      over tab icons which will emit signal closeRequest( QWidget * )
      when pressed.
    */
    void setHoverCloseButton( bool enable );

signals:
    /**
	 * The close button of a widget has been pressed (dependent upon
	 * hoverCloseButton()
    */
    void closeRequest( QWidget * );
#endif
};

#endif
