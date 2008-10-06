/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include "gpgop.h"

#include "gpgproc.h"

#include <QTimer>

namespace gpgQCAPlugin {

//----------------------------------------------------------------------------
// LineConverter
//----------------------------------------------------------------------------
class LineConverter
{
public:
	enum Mode { Read, Write };

	void setup(Mode m)
	{
		state = Normal;
		mode = m;
#ifdef Q_OS_WIN
		write_conv = true;
#else
		write_conv = false;
#endif
		prebytes = 0;
		list.clear();
	}

	QByteArray update(const QByteArray &buf)
	{
		if(mode == Read)
		{
			QByteArray out;

			if(state == Normal)
			{
				out = buf;
			}
			else
			{
				out.resize(buf.size() + 1);
				out[0] = '\r';
				memcpy(out.data() + 1, buf.data(), buf.size());
			}

			int n = 0;
			while(1)
			{
				n = out.indexOf('\r', n);
				// not found
				if(n == -1)
				{
					break;
				}
				// found, not last character
				if(n < (buf.size() - 1))
				{
					if(out[n + 1] == '\n')
					{
						// clip out the '\r'
						memmove(out.data() + n, out.data() + n + 1, out.size() - n - 1);
						out.resize(out.size() - 1);
					}
				}
				// found, last character
				else
				{
					state = Partial;
					break;
				}
				++n;
			}

			return out;
		}
		else
		{
			if(write_conv)
			{
				QByteArray out;
				int prev = 0;
				int at = 0;

				while(1)
				{
					int n = buf.indexOf('\n', at);
					if(n == -1)
						break;

					int chunksize = n - at;
					int oldsize = out.size();
					out.resize(oldsize + chunksize + 2);
					memcpy(out.data() + oldsize, buf.data() + at, chunksize);
					memcpy(out.data() + oldsize + chunksize, "\r\n", 2);

					list.append(prebytes + n + 1 - prev);
					prebytes = 0;
					prev = n;

					at = n + 1;
				}
				if(at < buf.size())
				{
					int chunksize = buf.size() - at;
					int oldsize = out.size();
					out.resize(oldsize + chunksize);
					memcpy(out.data() + oldsize, buf.data() + at, chunksize);
				}

				prebytes += buf.size() - prev;
				return out;
			}
			else
				return buf;
		}
	}

	QByteArray final()
	{
		if(mode == Read)
		{
			QByteArray out;
			if(state == Partial)
			{
				out.resize(1);
				out[0] = '\r';
			}
			return out;
		}
		else
		{
			return QByteArray();
		}
	}

	QByteArray process(const QByteArray &buf)
	{
		return update(buf) + final();
	}

	int writtenToActual(int bytes)
	{
		if(write_conv)
		{
			int n = 0;
			int counter = bytes;
			while(counter > 0)
			{
				if(!list.isEmpty() && bytes >= list.first())
				{
					++n;
					counter -= list.takeFirst();
				}
				else
				{
					if(list.isEmpty())
						prebytes -= counter;
					else
						list.first() -= counter;

					if(prebytes < 0)
					{
						bytes += prebytes;
						prebytes = 0;
					}

					break;
				}
			}
			return bytes - n;
		}
		else
			return bytes;
	}

private:
	enum State { Normal, Partial };
	Mode mode;
	State state;
	bool write_conv;
public:
	int prebytes;
	QList<int> list;
};

//----------------------------------------------------------------------------
// GpgAction
//----------------------------------------------------------------------------
static QDateTime getTimestamp(const QString &s)
{
	if(s.isEmpty())
		return QDateTime();

	if(s.contains('T'))
	{
		return QDateTime::fromString(s, Qt::ISODate);
	}
	else
	{
		QDateTime dt;
		dt.setTime_t(s.toInt());
		return dt;
	}
}

static QByteArray getCString(const QByteArray &a)
{
	QByteArray out;

	// convert the "backslash" C-string syntax
	for(int n = 0; n < a.size(); ++n)
	{
		if(a[n] == '\\' && n + 1 < a.size())
		{
			++n;
			unsigned char c = (unsigned char)a[n];
			if(c == '\\')
			{
				out += '\\';
			}
			else if(c == 'x' && n + 2 < a.size())
			{
				++n;
				QByteArray hex = a.mid(n, 2);
				++n; // only skip one, loop will skip the next

				bool ok;
				uint val = hex.toInt(&ok, 16);
				if(ok)
				{
					out += (unsigned char)val;
				}
				else
				{
					out += "\\x";
					out += hex;
				}
			}
		}
		else
		{
			out += a[n];
		}
	}

	return out;
}

static bool stringToKeyList(const QString &outstr, GpgOp::KeyList *_keylist, QString *_keyring)
{
	GpgOp::KeyList keyList;
	QStringList lines = outstr.split('\n');

	if(lines.count() < 1)
		return false;

	QStringList::ConstIterator it = lines.begin();

	// first line is keyring file
	QString keyring = *(it++);

	// if the second line isn't a divider, we are dealing
	// with a new version of gnupg that doesn't give us
	// the keyring file on gpg --list-keys --with-colons
	if(it == lines.end() || (*it).isEmpty() || (*it).at(0) != '-')
	{
		// first line wasn't the keyring name...
		keyring.clear();
		// ...so read the first line again
		it--;
	}
	else
	{
		// this was the divider line - skip it
		it++;
	}

	for(; it != lines.end(); ++it)
	{
		QStringList f = (*it).split(':');
		if(f.count() < 1)
			continue;
		QString type = f[0];

		bool key = false; // key or not
		bool primary = false; // primary key or sub key
		bool sec = false; // private key or not

		if(type == "pub")
		{
			key = true;
			primary = true;
		}
		else if(type == "sec")
		{
			key = true;
			primary = true;
			sec = true;
		}
		else if(type == "sub")
		{
			key = true;
		}
		else if(type == "ssb")
		{
			key = true;
			sec = true;
		}

		if(key)
		{
			if(primary)
			{
				keyList += GpgOp::Key();

				QString trust = f[1];
				if(trust == "f" || trust == "u")
					keyList.last().isTrusted = true;
			}

			int key_type = f[3].toInt();
			QString caps = f[11];

			GpgOp::KeyItem item;
			item.bits = f[2].toInt();
			if(key_type == 1)
				item.type = GpgOp::KeyItem::RSA;
			else if(key_type == 16)
				item.type = GpgOp::KeyItem::ElGamal;
			else if(key_type == 17)
				item.type = GpgOp::KeyItem::DSA;
			else
				item.type = GpgOp::KeyItem::Unknown;
			item.id = f[4];
			item.creationDate = getTimestamp(f[5]);
			item.expirationDate = getTimestamp(f[6]);
			if(caps.contains('e'))
				item.caps |= GpgOp::KeyItem::Encrypt;
			if(caps.contains('s'))
				item.caps |= GpgOp::KeyItem::Sign;
			if(caps.contains('c'))
				item.caps |= GpgOp::KeyItem::Certify;
			if(caps.contains('a'))
				item.caps |= GpgOp::KeyItem::Auth;

			keyList.last().keyItems += item;
		}
		else if(type == "uid")
		{
			QByteArray uid = getCString(f[9].toLatin1());
			keyList.last().userIds.append(QString::fromUtf8(uid));
		}
		else if(type == "fpr")
		{
			QString s = f[9];
			keyList.last().keyItems.last().fingerprint = s;
		}
	}

	if(_keylist)
		*_keylist = keyList;
	if(_keyring)
		*_keyring = keyring;

	return true;
}

static bool findKeyringFilename(const QString &outstr, QString *_keyring)
{
	QStringList lines = outstr.split('\n');
	if(lines.count() < 1)
		return false;

	*_keyring = lines[0];
	return true;
}

class GpgAction : public QObject
{
	Q_OBJECT
public:
	class Input
	{
	public:
		QString bin;
		GpgOp::Type op;
		bool opt_ascii, opt_noagent, opt_alwaystrust;
		QString opt_pubfile, opt_secfile;
		QStringList recip_ids;
		QString signer_id;
		QByteArray sig;
		QByteArray inkey;
		QString export_key_id;
		QString delete_key_fingerprint;

		Input() : opt_ascii(false), opt_noagent(false), opt_alwaystrust(false) {}
	};

	class Output
	{
	public:
		bool success;
		GpgOp::Error errorCode;
		GpgOp::KeyList keys;
		QString keyringFile;
		QString encryptedToId;
		bool wasSigned;
		QString signerId;
		QDateTime timestamp;
		GpgOp::VerifyResult verifyResult;

		Output() : success(false), errorCode(GpgOp::ErrorUnknown), wasSigned(false) {}
	};

	Input input;
	Output output;

	GPGProc proc;
	bool collectOutput, allowInput;
	LineConverter readConv, writeConv;
	bool readText, writeText;
	QByteArray buf_stdout, buf_stderr;
	bool useAux;
	QString passphraseKeyId;
	bool signing, signPartDone, decryptGood, signGood;
	GpgOp::Error curError;
	bool badPassphrase;
	bool need_submitPassphrase, need_cardOkay;
	QString diagnosticText;
	SafeTimer dtextTimer;

#ifdef GPG_PROFILE
	QTime timer;
#endif

	GpgAction(QObject *parent = 0) : QObject(parent), proc(this), dtextTimer(this)
	{
		dtextTimer.setSingleShot(true);

		connect(&proc, SIGNAL(error(gpgQCAPlugin::GPGProc::Error)), SLOT(proc_error(gpgQCAPlugin::GPGProc::Error)));
		connect(&proc, SIGNAL(finished(int)), SLOT(proc_finished(int)));
		connect(&proc, SIGNAL(readyReadStdout()), SLOT(proc_readyReadStdout()));
		connect(&proc, SIGNAL(readyReadStderr()), SLOT(proc_readyReadStderr()));
		connect(&proc, SIGNAL(readyReadStatusLines()), SLOT(proc_readyReadStatusLines()));
		connect(&proc, SIGNAL(bytesWrittenStdin(int)), SLOT(proc_bytesWrittenStdin(int)));
		connect(&proc, SIGNAL(bytesWrittenAux(int)), SLOT(proc_bytesWrittenAux(int)));
		connect(&proc, SIGNAL(bytesWrittenCommand(int)), SLOT(proc_bytesWrittenCommand(int)));
		connect(&proc, SIGNAL(debug(const QString &)), SLOT(proc_debug(const QString &)));
		connect(&dtextTimer, SIGNAL(timeout()), SLOT(t_dtext()));

		reset();
	}

	~GpgAction()
	{
		reset();
	}

	void reset()
	{
		collectOutput = true;
		allowInput = false;
		readConv.setup(LineConverter::Read);
		writeConv.setup(LineConverter::Write);
		readText = false;
		writeText = false;
		useAux = false;
		passphraseKeyId = QString();
		signing = false;
		signPartDone = false;
		decryptGood = false;
		signGood = false;
		curError = GpgOp::ErrorUnknown;
		badPassphrase = false;
		need_submitPassphrase = false;
		need_cardOkay = false;
		diagnosticText = QString();
		dtextTimer.stop();

		output = Output();

		proc.reset();
	}

	void start()
	{
		reset();

		QStringList args;
		bool extra = false;

		if(input.opt_ascii)
			args += "--armor";

		if(input.opt_noagent)
			args += "--no-use-agent";

		if(input.opt_alwaystrust)
			args += "--always-trust";

		if(!input.opt_pubfile.isEmpty() && !input.opt_secfile.isEmpty())
		{
			args += "--no-default-keyring";
			args += "--keyring";
			args += input.opt_pubfile;
			args += "--secret-keyring";
			args += input.opt_secfile;
		}

		switch(input.op)
		{
			case GpgOp::Check:
			{
				args += "--version";
				readText = true;
				break;
			}
			case GpgOp::SecretKeyringFile:
			{
				args += "--list-secret-keys";
				readText = true;
				break;
			}
			case GpgOp::PublicKeyringFile:
			{
				args += "--list-public-keys";
				readText = true;
				break;
			}
			case GpgOp::SecretKeys:
			{
				args += "--fixed-list-mode";
				args += "--with-colons";
				args += "--with-fingerprint";
				args += "--with-fingerprint";
				args += "--list-secret-keys";
				readText = true;
				break;
			}
			case GpgOp::PublicKeys:
			{
				args += "--fixed-list-mode";
				args += "--with-colons";
				args += "--with-fingerprint";
				args += "--with-fingerprint";
				args += "--list-public-keys";
				readText = true;
				break;
			}
			case GpgOp::Encrypt:
			{
				args += "--encrypt";

				// recipients
				for(QStringList::ConstIterator it = input.recip_ids.begin(); it != input.recip_ids.end(); ++it)
				{
					args += "--recipient";
					args += QString("0x") + *it;
				}
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					readText = true;
				break;
			}
			case GpgOp::Decrypt:
			{
				args += "--decrypt";
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					writeText = true;
				break;
			}
			case GpgOp::Sign:
			{
				args += "--default-key";
				args += QString("0x") + input.signer_id;
				args += "--sign";
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					readText = true;
				signing = true;
				break;
			}
			case GpgOp::SignAndEncrypt:
			{
				args += "--default-key";
				args += QString("0x") + input.signer_id;
				args += "--sign";
				args += "--encrypt";

				// recipients
				for(QStringList::ConstIterator it = input.recip_ids.begin(); it != input.recip_ids.end(); ++it)
				{
					args += "--recipient";
					args += QString("0x") + *it;
				}
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					readText = true;
				signing = true;
				break;
			}
			case GpgOp::SignClearsign:
			{
				args += "--default-key";
				args += QString("0x") + input.signer_id;
				args += "--clearsign";
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					readText = true;
				signing = true;
				break;
			}
			case GpgOp::SignDetached:
			{
				args += "--default-key";
				args += QString("0x") + input.signer_id;
				args += "--detach-sign";
				extra = true;
				collectOutput = false;
				allowInput = true;
				if(input.opt_ascii)
					readText = true;
				signing = true;
				break;
			}
			case GpgOp::Verify:
			{
				args += "--verify";
				args += "-"; //krazy:exclude=doublequote_chars
				extra = true;
				allowInput = true;
				if(input.opt_ascii)
					writeText = true;
				break;
			}
			case GpgOp::VerifyDetached:
			{
				args += "--verify";
				args += "-"; //krazy:exclude=doublequote_chars
				args += "-&?";
				extra = true;
				allowInput = true;
				useAux = true;
				break;
			}
			case GpgOp::Import:
			{
				args += "--import";
				readText = true;
				if(input.opt_ascii)
					writeText = true;
				break;
			}
			case GpgOp::Export:
			{
				args += "--export";
				args += QString("0x") + input.export_key_id;
				collectOutput = false;
				if(input.opt_ascii)
					readText = true;
				break;
			}
			case GpgOp::DeleteKey:
			{
				args += "--batch";
				args += "--delete-key";
				args += QString("0x") + input.delete_key_fingerprint;
				break;
			}
		}

#ifdef GPG_PROFILE
		timer.start();
		printf("<< launch >>\n");
#endif
		proc.start(input.bin, args, extra ? GPGProc::ExtendedMode : GPGProc::NormalMode);

		// detached sig
		if(input.op == GpgOp::VerifyDetached)
		{
			QByteArray a = input.sig;
			if(input.opt_ascii)
			{
				LineConverter conv;
				conv.setup(LineConverter::Write);
				a = conv.process(a);
			}
			proc.writeStdin(a);
			proc.closeStdin();
		}

		// import
		if(input.op == GpgOp::Import)
		{
			QByteArray a = input.inkey;
			if(writeText)
			{
				LineConverter conv;
				conv.setup(LineConverter::Write);
				a = conv.process(a);
			}
			proc.writeStdin(a);
			proc.closeStdin();
		}
	}

#ifdef QPIPE_SECURE
	void submitPassphrase(const QCA::SecureArray &a)
#else
	void submitPassphrase(const QByteArray &a)
#endif
	{
		if(!need_submitPassphrase)
			return;

		need_submitPassphrase = false;

#ifdef QPIPE_SECURE
		QCA::SecureArray b;
#else
		QByteArray b;
#endif
		// filter out newlines, since that's the delimiter used
		// to indicate a submitted passphrase
		b.resize(a.size());
		int at = 0;
		for(int n = 0; n < a.size(); ++n)
		{
			if(a[n] != '\n')
				b[at++] = a[n];
		}
		b.resize(at);

		// append newline
		b.resize(b.size() + 1);
		b[b.size() - 1] = '\n';
		proc.writeCommand(b);
	}

public slots:
	QByteArray read()
	{
		if(collectOutput)
			return QByteArray();

		QByteArray a = proc.readStdout();
		if(readText)
			a = readConv.update(a);
		if(!proc.isActive())
			a += readConv.final();
		return a;
	}

	void write(const QByteArray &in)
	{
		if(!allowInput)
			return;

		QByteArray a = in;
		if(writeText)
			a = writeConv.update(in);

		if(useAux)
			proc.writeAux(a);
		else
			proc.writeStdin(a);
	}

	void endWrite()
	{
		if(!allowInput)
			return;

		if(useAux)
			proc.closeAux();
		else
			proc.closeStdin();
	}

	void cardOkay()
	{
		if(need_cardOkay)
		{
			need_cardOkay = false;
			submitCommand("\n");
		}
	}

	QString readDiagnosticText()
	{
		QString s = diagnosticText;
		diagnosticText = QString();
		return s;
	}

signals:
	void readyRead();
	void bytesWritten(int bytes);
	void finished();
	void needPassphrase(const QString &keyId);
	void needCard();
	void readyReadDiagnosticText();

private:
	void submitCommand(const QByteArray &a)
	{
		proc.writeCommand(a);
	}

	// since str is taken as a value, it is ok to use the same variable for 'rest'
	QString nextArg(QString str, QString *rest = 0)
	{
		QString out;
		int n = str.indexOf(' ');
		if(n == -1)
		{
			if(rest)
				*rest = QString();
			return str;
		}
		else
		{
			if(rest)
				*rest = str.mid(n + 1);
			return str.mid(0, n);
		}
	}

	void processStatusLine(const QString &line)
	{
		diagnosticText += QString("{") + line + "}\n";
		ensureDTextEmit();

		if(!proc.isActive())
			return;

		QString s, rest;
		s = nextArg(line, &rest);

		if(s == "NODATA")
		{
			// only set this if it'll make it better
			if(curError == GpgOp::ErrorUnknown)
				curError = GpgOp::ErrorFormat;
		}
		else if(s == "UNEXPECTED")
		{
			if(curError == GpgOp::ErrorUnknown)
				curError = GpgOp::ErrorFormat;
		}
		else if(s == "KEYEXPIRED")
		{
			if(curError == GpgOp::ErrorUnknown)
			{
				if(input.op == GpgOp::SignAndEncrypt)
				{
					if(!signPartDone)
						curError = GpgOp::ErrorSignerExpired;
					else
						curError = GpgOp::ErrorEncryptExpired;
				}
				else
				{
					if(signing)
						curError = GpgOp::ErrorSignerExpired;
					else
						curError = GpgOp::ErrorEncryptExpired;
				}
			}
		}
		else if(s == "INV_RECP")
		{
			int r = nextArg(rest).toInt();

			if(curError == GpgOp::ErrorUnknown)
			{
				if(r == 10)
					curError = GpgOp::ErrorEncryptUntrusted;
				else
					curError = GpgOp::ErrorEncryptInvalid;
			}
		}
		else if(s == "NO_SECKEY")
		{
			output.encryptedToId = nextArg(rest);

			if(curError == GpgOp::ErrorUnknown)
				curError = GpgOp::ErrorDecryptNoKey;
		}
		else if(s == "DECRYPTION_OKAY")
		{
			decryptGood = true;

			// message could be encrypted with several keys
			if(curError == GpgOp::ErrorDecryptNoKey)
				curError = GpgOp::ErrorUnknown;
		}
		else if(s == "SIG_CREATED")
		{
			signGood = true;
		}
		else if(s == "USERID_HINT")
		{
			passphraseKeyId = nextArg(rest);
		}
		else if(s == "GET_HIDDEN")
		{
			QString arg = nextArg(rest);
			if(arg == "passphrase.enter" || arg == "passphrase.pin.ask")
			{
				need_submitPassphrase = true;

				// for signal-safety, emit later
				QMetaObject::invokeMethod(this, "needPassphrase", Qt::QueuedConnection, Q_ARG(QString, passphraseKeyId));
			}
		}
		else if(s == "GET_LINE")
		{
			QString arg = nextArg(rest);
			if(arg == "cardctrl.insert_card.okay")
			{
				need_cardOkay = true;

				QMetaObject::invokeMethod(this, "needCard", Qt::QueuedConnection);
			}
		}
		else if(s == "GET_BOOL")
		{
			QString arg = nextArg(rest);
			if(arg == "untrusted_key.override")
				submitCommand("no\n");
		}
		else if(s == "GOOD_PASSPHRASE")
		{
			badPassphrase = false;

			// a trick to determine what KEYEXPIRED should apply to
			signPartDone = true;
		}
		else if(s == "BAD_PASSPHRASE")
		{
			badPassphrase = true;
		}
		else if(s == "GOODSIG")
		{
			output.wasSigned = true;
			output.signerId = nextArg(rest);
			output.verifyResult = GpgOp::VerifyGood;
		}
		else if(s == "BADSIG")
		{
			output.wasSigned = true;
			output.signerId = nextArg(rest);
			output.verifyResult = GpgOp::VerifyBad;
		}
		else if(s == "ERRSIG")
		{
			output.wasSigned = true;
			QStringList list = rest.split(' ', QString::SkipEmptyParts);
			output.signerId = list[0];
			output.timestamp = getTimestamp(list[4]);
			output.verifyResult = GpgOp::VerifyNoKey;
		}
		else if(s == "VALIDSIG")
		{
			QStringList list = rest.split(' ', QString::SkipEmptyParts);
			output.timestamp = getTimestamp(list[2]);
		}
	}

	void processResult(int code)
	{
#ifdef GPG_PROFILE
		printf("<< launch: %d >>\n", timer.elapsed());
#endif

		// put stdout and stderr into QStrings
		QString outstr = QString::fromLatin1(buf_stdout);
		QString errstr = QString::fromLatin1(buf_stderr);

		if(collectOutput)
			diagnosticText += QString("stdout: [%1]\n").arg(outstr);
		diagnosticText += QString("stderr: [%1]\n").arg(errstr);
		ensureDTextEmit();

		if(badPassphrase)
		{
			output.errorCode = GpgOp::ErrorPassphrase;
		}
		else if(curError != GpgOp::ErrorUnknown)
		{
			output.errorCode = curError;
		}
		else if(code == 0)
		{
			if(input.op == GpgOp::SecretKeyringFile || input.op == GpgOp::PublicKeyringFile)
			{
				if(findKeyringFilename(outstr, &output.keyringFile))
					output.success = true;
			}
			else if(input.op == GpgOp::SecretKeys || input.op == GpgOp::PublicKeys)
			{
				if(stringToKeyList(outstr, &output.keys, &output.keyringFile))
					output.success = true;
			}
			else
				output.success = true;
		}
		else
		{
			// decrypt and sign success based on status only.
			// this is mainly because gpg uses fatal return
			// values if there is trouble with gpg-agent, even
			// though the operation otherwise works.

			if(input.op == GpgOp::Decrypt && decryptGood)
				output.success = true;
			if(signing && signGood)
				output.success = true;

			// gpg will indicate failure for bad sigs, but we don't
			// consider this to be operation failure.

			bool signedMakesItGood = false;
			if(input.op == GpgOp::Verify || input.op == GpgOp::VerifyDetached)
				signedMakesItGood = true;

			if(signedMakesItGood && output.wasSigned)
				output.success = true;
		}

		emit finished();
	}

	void ensureDTextEmit()
	{
		if(!dtextTimer.isActive())
			dtextTimer.start();
	}

private slots:
	void t_dtext()
	{
		emit readyReadDiagnosticText();
	}

	void proc_error(gpgQCAPlugin::GPGProc::Error e)
	{
		QString str;
		if(e == GPGProc::FailedToStart)
			str = "FailedToStart";
		else if(e == GPGProc::UnexpectedExit)
			str = "UnexpectedExit";
		else if(e == GPGProc::ErrorWrite)
			str = "ErrorWrite";

		diagnosticText += QString("GPG Process Error: %1\n").arg(str);
		ensureDTextEmit();

		output.errorCode = GpgOp::ErrorProcess;
		emit finished();
	}

	void proc_finished(int exitCode)
	{
		diagnosticText += QString("GPG Process Finished: exitStatus=%1\n").arg(exitCode);
		ensureDTextEmit();

		processResult(exitCode);
	}

	void proc_readyReadStdout()
	{
		if(collectOutput)
		{
			QByteArray a = proc.readStdout();
			if(readText)
				a = readConv.update(a);
			buf_stdout.append(a);
		}
		else
			emit readyRead();
	}

	void proc_readyReadStderr()
	{
		buf_stderr.append(proc.readStderr());
	}

	void proc_readyReadStatusLines()
	{
		QStringList lines = proc.readStatusLines();
		for(int n = 0; n < lines.count(); ++n)
			processStatusLine(lines[n]);
	}

	void proc_bytesWrittenStdin(int bytes)
	{
		if(!useAux)
		{
			int actual = writeConv.writtenToActual(bytes);
			emit bytesWritten(actual);
		}
	}

	void proc_bytesWrittenAux(int bytes)
	{
		if(useAux)
		{
			int actual = writeConv.writtenToActual(bytes);
			emit bytesWritten(actual);
		}
	}

	void proc_bytesWrittenCommand(int)
	{
		// don't care about this
	}

	void proc_debug(const QString &str)
	{
		diagnosticText += "GPGProc: " + str + '\n';
		ensureDTextEmit();
	}
};

//----------------------------------------------------------------------------
// GpgOp
//----------------------------------------------------------------------------
enum ResetMode
{
	ResetSession        = 0,
	ResetSessionAndData = 1,
	ResetAll            = 2
};

class GpgOp::Private : public QObject
{
	Q_OBJECT
public:
	QCA::Synchronizer sync;
	GpgOp *q;
	GpgAction *act;
	QString bin;
	GpgOp::Type op;
	GpgAction::Output output;
	QByteArray result;
	QString diagnosticText;
	QList<GpgOp::Event> eventList;
	bool waiting;

	bool opt_ascii, opt_noagent, opt_alwaystrust;
	QString opt_pubfile, opt_secfile;

#ifdef GPG_PROFILE
	QTime timer;
#endif

	Private(GpgOp *_q) : QObject(_q), sync(_q), q(_q)
	{
		act = 0;
		waiting = false;

		reset(ResetAll);
	}

	~Private()
	{
		reset(ResetAll);
	}

	void reset(ResetMode mode)
	{
		if(act)
		{
			releaseAndDeleteLater(this, act);
			act = 0;
		}

		if(mode >= ResetSessionAndData)
		{
			output = GpgAction::Output();
			result.clear();
			diagnosticText = QString();
			eventList.clear();
		}

		if(mode >= ResetAll)
		{
			opt_ascii = false;
			opt_noagent = false;
			opt_alwaystrust = false;
			opt_pubfile = QString();
			opt_secfile = QString();
		}
	}

	void make_act(GpgOp::Type _op)
	{
		reset(ResetSessionAndData);

		op = _op;

		act = new GpgAction(this);

		connect(act, SIGNAL(readyRead()), SLOT(act_readyRead()));
		connect(act, SIGNAL(bytesWritten(int)), SLOT(act_bytesWritten(int)));
		connect(act, SIGNAL(needPassphrase(const QString &)), SLOT(act_needPassphrase(const QString &)));
		connect(act, SIGNAL(needCard()), SLOT(act_needCard()));
		connect(act, SIGNAL(finished()), SLOT(act_finished()));
		connect(act, SIGNAL(readyReadDiagnosticText()), SLOT(act_readyReadDiagnosticText()));

		act->input.bin = bin;
		act->input.op = op;
		act->input.opt_ascii = opt_ascii;
		act->input.opt_noagent = opt_noagent;
		act->input.opt_alwaystrust = opt_alwaystrust;
		act->input.opt_pubfile = opt_pubfile;
		act->input.opt_secfile = opt_secfile;
	}

	void eventReady(const GpgOp::Event &e)
	{
		eventList += e;
		sync.conditionMet();
	}

	void eventReady(GpgOp::Event::Type type)
	{
		GpgOp::Event e;
		e.type = type;
		eventReady(e);
	}

	void eventReady(GpgOp::Event::Type type, int written)
	{
		GpgOp::Event e;
		e.type = type;
		e.written = written;
		eventReady(e);
	}

	void eventReady(GpgOp::Event::Type type, const QString &keyId)
	{
		GpgOp::Event e;
		e.type = type;
		e.keyId = keyId;
		eventReady(e);
	}

public slots:
	void act_readyRead()
	{
		if(waiting)
			eventReady(GpgOp::Event::ReadyRead);
		else
			emit q->readyRead();
	}

	void act_bytesWritten(int bytes)
	{
		if(waiting)
			eventReady(GpgOp::Event::BytesWritten, bytes);
		else
			emit q->bytesWritten(bytes);
	}

	void act_needPassphrase(const QString &keyId)
	{
		if(waiting)
			eventReady(GpgOp::Event::NeedPassphrase, keyId);
		else
			emit q->needPassphrase(keyId);
	}

	void act_needCard()
	{
		if(waiting)
			eventReady(GpgOp::Event::NeedCard);
		else
			emit q->needCard();
	}

	void act_readyReadDiagnosticText()
	{
		QString s = act->readDiagnosticText();
		//printf("dtext ready: [%s]\n", qPrintable(s));
		diagnosticText += s;

		if(waiting)
			eventReady(GpgOp::Event::ReadyReadDiagnosticText);
		else
			emit q->readyReadDiagnosticText();
	}

	void act_finished()
	{
#ifdef GPG_PROFILE
		if(op == GpgOp::Encrypt)
			printf("<< doEncrypt: %d >>\n", timer.elapsed());
#endif

		result = act->read();
		diagnosticText += act->readDiagnosticText();
		output = act->output;

		QMap<int, QString> errmap;
		errmap[GpgOp::ErrorProcess] = "ErrorProcess";
		errmap[GpgOp::ErrorPassphrase] = "ErrorPassphrase";
		errmap[GpgOp::ErrorFormat] = "ErrorFormat";
		errmap[GpgOp::ErrorSignerExpired] = "ErrorSignerExpired";
		errmap[GpgOp::ErrorEncryptExpired] = "ErrorEncryptExpired";
		errmap[GpgOp::ErrorEncryptUntrusted] = "ErrorEncryptUntrusted";
		errmap[GpgOp::ErrorEncryptInvalid] = "ErrorEncryptInvalid";
		errmap[GpgOp::ErrorDecryptNoKey] = "ErrorDecryptNoKey";
		errmap[GpgOp::ErrorUnknown] = "ErrorUnknown";
		if(output.success)
			diagnosticText += "GpgAction success\n";
		else
			diagnosticText += QString("GpgAction error: %1\n").arg(errmap[output.errorCode]);

		if(output.wasSigned)
		{
			QString s;
			if(output.verifyResult == GpgOp::VerifyGood)
				s = "VerifyGood";
			else if(output.verifyResult == GpgOp::VerifyBad)
				s = "VerifyBad";
			else
				s = "VerifyNoKey";
			diagnosticText += QString("wasSigned: verifyResult: %1\n").arg(s);
		}

		//printf("diagnosticText:\n%s", qPrintable(diagnosticText));

		reset(ResetSession);

		if(waiting)
			eventReady(GpgOp::Event::Finished);
		else
			emit q->finished();
	}
};

GpgOp::GpgOp(const QString &bin, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	d->bin = bin;
}

GpgOp::~GpgOp()
{
	delete d;
}

void GpgOp::reset()
{
	d->reset(ResetAll);
}

bool GpgOp::isActive() const
{
	return (d->act ? true : false);
}

GpgOp::Type GpgOp::op() const
{
	return d->op;
}

void GpgOp::setAsciiFormat(bool b)
{
	d->opt_ascii = b;
}

void GpgOp::setDisableAgent(bool b)
{
	d->opt_noagent = b;
}

void GpgOp::setAlwaysTrust(bool b)
{
	d->opt_alwaystrust = b;
}

void GpgOp::setKeyrings(const QString &pubfile, const QString &secfile)
{
	d->opt_pubfile = pubfile;
	d->opt_secfile = secfile;
}

void GpgOp::doCheck()
{
	d->make_act(Check);
	d->act->start();
}

void GpgOp::doSecretKeyringFile()
{
	d->make_act(SecretKeyringFile);
	d->act->start();
}

void GpgOp::doPublicKeyringFile()
{
	d->make_act(PublicKeyringFile);
	d->act->start();
}

void GpgOp::doSecretKeys()
{
	d->make_act(SecretKeys);
	d->act->start();
}

void GpgOp::doPublicKeys()
{
	d->make_act(PublicKeys);
	d->act->start();
}

void GpgOp::doEncrypt(const QStringList &recip_ids)
{
#ifdef GPG_PROFILE
	d->timer.start();
	printf("<< doEncrypt >>\n");
#endif

	d->make_act(Encrypt);
	d->act->input.recip_ids = recip_ids;
	d->act->start();
}

void GpgOp::doDecrypt()
{
	d->make_act(Decrypt);
	d->act->start();
}

void GpgOp::doSign(const QString &signer_id)
{
	d->make_act(Sign);
	d->act->input.signer_id = signer_id;
	d->act->start();
}

void GpgOp::doSignAndEncrypt(const QString &signer_id, const QStringList &recip_ids)
{
	d->make_act(SignAndEncrypt);
	d->act->input.signer_id = signer_id;
	d->act->input.recip_ids = recip_ids;
	d->act->start();
}

void GpgOp::doSignClearsign(const QString &signer_id)
{
	d->make_act(SignClearsign);
	d->act->input.signer_id = signer_id;
	d->act->start();
}

void GpgOp::doSignDetached(const QString &signer_id)
{
	d->make_act(SignDetached);
	d->act->input.signer_id = signer_id;
	d->act->start();
}

void GpgOp::doVerify()
{
	d->make_act(Verify);
	d->act->start();
}

void GpgOp::doVerifyDetached(const QByteArray &sig)
{
	d->make_act(VerifyDetached);
	d->act->input.sig = sig;
	d->act->start();
}

void GpgOp::doImport(const QByteArray &in)
{
	d->make_act(Import);
	d->act->input.inkey = in;
	d->act->start();
}

void GpgOp::doExport(const QString &key_id)
{
	d->make_act(Export);
	d->act->input.export_key_id = key_id;
	d->act->start();
}

void GpgOp::doDeleteKey(const QString &key_fingerprint)
{
	d->make_act(DeleteKey);
	d->act->input.delete_key_fingerprint = key_fingerprint;
	d->act->start();
}

#ifdef QPIPE_SECURE
void GpgOp::submitPassphrase(const QCA::SecureArray &a)
#else
void GpgOp::submitPassphrase(const QByteArray &a)
#endif
{
	d->act->submitPassphrase(a);
}

void GpgOp::cardOkay()
{
	d->act->cardOkay();
}

QByteArray GpgOp::read()
{
	if(d->act)
	{
		return d->act->read();
	}
	else
	{
		QByteArray a = d->result;
		d->result.clear();
		return a;
	}
}

void GpgOp::write(const QByteArray &in)
{
	d->act->write(in);
}

void GpgOp::endWrite()
{
	d->act->endWrite();
}

QString GpgOp::readDiagnosticText()
{
	QString s = d->diagnosticText;
	d->diagnosticText = QString();
	return s;
}

GpgOp::Event GpgOp::waitForEvent(int msecs)
{
	if(!d->eventList.isEmpty())
		return d->eventList.takeFirst();

	if(!d->act)
		return GpgOp::Event();

	d->waiting = true;
	d->sync.waitForCondition(msecs);
	d->waiting = false;
	return d->eventList.takeFirst();
}

bool GpgOp::success() const
{
	return d->output.success;
}

GpgOp::Error GpgOp::errorCode() const
{
	return d->output.errorCode;
}

GpgOp::KeyList GpgOp::keys() const
{
	return d->output.keys;
}

QString GpgOp::keyringFile() const
{
	return d->output.keyringFile;
}

QString GpgOp::encryptedToId() const
{
	return d->output.encryptedToId;
}

bool GpgOp::wasSigned() const
{
	return d->output.wasSigned;
}

QString GpgOp::signerId() const
{
	return d->output.signerId;
}

QDateTime GpgOp::timestamp() const
{
	return d->output.timestamp;
}

GpgOp::VerifyResult GpgOp::verifyResult() const
{
	return d->output.verifyResult;
}

}

#include "gpgop.moc"
