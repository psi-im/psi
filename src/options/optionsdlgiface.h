#ifndef OPTIONSDLGIFACE_H
#define OPTIONSDLGIFACE_H

class OptionsDlgIface : public QObject
{
	Q_OBJECT
public:
	OptionsDlgIface(QObject *parent, const char *name);
	~OptionsDlgIface();

public slots:

};

#endif
