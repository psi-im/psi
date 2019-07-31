/*
 * Copyright (C) 2008  Martin Hostettler
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// simple UI integration for mini command system.

#ifndef MINICMDSIMPLESITE_H
#define MINICMDSIMPLESITE_H

#include <QPalette>

#include "minicmd.h"

class QLabel;
class QString;
class QTextEdit;

class MCmdSimpleSite : public MCmdUiSiteIface {
public:
    MCmdSimpleSite(QLabel *p, QTextEdit *i);
    MCmdSimpleSite();
    ~MCmdSimpleSite() = default;
    virtual void mCmdReady(const QString prompt, const QString preset);
    virtual void mCmdClose();
    bool isActive() const;
    void setPrompt(QLabel *p);
    void setInput(QTextEdit *i);
protected:
    QLabel *promptWidget;
    QTextEdit *inputWidget;
    bool open;
    QString mini_msg_swap;
    QPalette palette, cmdPalette;
};

#endif
