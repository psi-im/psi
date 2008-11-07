#include "QtCrypto.h"

namespace QCA {

CertificateCollection gSystemStore;

CertificateCollection systemStore() 
{
	return gSystemStore;
}

}
