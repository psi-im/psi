#ifndef MOCKQCA_QTCRYPTO
#define MOCKQCA_QTCRYPTO

#include <QList>
#include <QByteArray>
#include <QString>

namespace QCA {
	enum Validity
	{
		ValidityGood,						 
		ErrorRejected,					 
		ErrorUntrusted,					 
		ErrorSignatureFailed,		 
		ErrorInvalidCA,					 
		ErrorInvalidPurpose,		 
		ErrorSelfSigned,				 
		ErrorRevoked,						 
		ErrorPathLengthExceeded, 
		ErrorExpired,						 
		ErrorExpiredCA,					 
		ErrorValidityUnknown = 64
	};

	enum ConvertResult { ConvertGood };

	class Certificate 
	{
		public:
			Certificate(const QString& id = "") { 
				id_ = id; 
			}

			bool operator==(const Certificate& other) {
				return other.id_ == id_;
			}

			static Certificate fromPEMFile(const QString&, ConvertResult* result) { 
				*result = ConvertGood; 
				return Certificate(); 
			}

			static Certificate fromDER(const QString&, ConvertResult* result) { 
				*result = ConvertGood; 
				return Certificate(); 
			}

		private:
			QString id_;
	};

	class CertificateCollection : public QList<Certificate> 
	{
		public: 
			CertificateCollection() {}
			void addCertificate(const Certificate& c) { *this += c; }
			QList<Certificate> certificates() const { return *this; }
	};

	class SecureArray
	{
		public:
			SecureArray() {}
			QByteArray toByteArray() { return QByteArray(); }
	};

	class Base64
	{
		public:
			Base64() {}
			SecureArray stringToArray(const QString&) { return SecureArray(); }
	};

	class TLS
	{
		public:
			enum Result { NoCertificate, Valid, HostMismatch, InvalidCertificate };
	};

	extern CertificateCollection gSystemStore;
	CertificateCollection systemStore();
}


#endif
