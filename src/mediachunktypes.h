#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <functional>
#include <utility>

// Control flow uses the code; message is diagnostic text only.
class MediaChunkError {
public:
    enum Code { None, Failed, Cancelled, RangeUnsupported };
    MediaChunkError() = default;
    MediaChunkError(QString message) : code_(message.isEmpty() ? None : Failed), message_(std::move(message)) { }
    MediaChunkError(Code code, QString message) : code_(code), message_(std::move(message)) { }
    bool           isEmpty() const { return code_ == None; }
    Code           code() const { return code_; }
    const QString &message() const { return message_; }

private:
    Code    code_ = None;
    QString message_;
};
Q_DECLARE_METATYPE(MediaChunkError)

inline constexpr quint64 MediaChunkMaxSize = 4 * 1024 * 1024;
using MediaChunkCompletion                 = std::function<void(QByteArray, MediaChunkError)>;
using MediaChunkAbort                      = std::function<void()>;
