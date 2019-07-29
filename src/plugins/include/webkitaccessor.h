#ifndef WEBKITACCESSOR_H
#define WEBKITACCESSOR_H

class WebkitAccessingHost;

class WebkitAccessor
{
public:
    virtual ~WebkitAccessor() {}

    virtual void setWebkitAccessingHost(WebkitAccessingHost* host) = 0;

};

Q_DECLARE_INTERFACE(WebkitAccessor, "org.psi-im.WebkitAccessor/0.1");

#endif // WEBKITACCESSOR_H
