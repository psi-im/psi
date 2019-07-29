/*
 * psiactionlist.h - the customizeable action list for Psi
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2004  Michail Pishchagin
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSIACTIONLIST_H
#define PSIACTIONLIST_H

#include "actionlist.h"

class PsiCon;

class PsiActionList : public MetaActionList
{
public:
    PsiActionList(PsiCon *psi);
    ~PsiActionList();

    enum ActionsType {
        Actions_Common    = (1 << 0),
        Actions_MainWin   = (1 << 1),
        // Actions_Message   = (1 << 2),
        Actions_Chat      = (1 << 3),
        Actions_Groupchat = (1 << 4)
    };

public:
    class Private;
private:
    Private *d;
};

#endif
