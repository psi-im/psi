#ifndef PSIACCOUNTCONTROLLER_H
#define PSIACCOUNTCONTROLLER_H

class PsiAccountControllingHost;

class PsiAccountController
{
public:
	virtual ~PsiAccountController() {}

	virtual void setPsiAccountControllingHost(PsiAccountControllingHost* host) = 0;

};

Q_DECLARE_INTERFACE(PsiAccountController, "org.psi-im.PsiAccountController/0.1");


#endif // PSIACCOUNTCONTROLLER_H
