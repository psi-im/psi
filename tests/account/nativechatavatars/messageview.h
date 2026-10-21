/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>

class MessageView {
public:
    MessageView(bool local, const QString &nick = {}) : local_(local), nick_(nick) { }
    bool           isLocal() const { return local_; }
    const QString &nick() const { return nick_; }

private:
    bool    local_;
    QString nick_;
};
