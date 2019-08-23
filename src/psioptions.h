/*
 * psioptions.h - Psi options class
 * Copyright (C) 2006  Kevin Smith
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

#ifndef PSIOPTIONS_H
#define PSIOPTIONS_H

#include "optionstree.h"

// Some hard coded options
#define MINIMUM_OPACITY 10

class QString;
class QTimer;

namespace XMPP {
    class Client;
}

class PsiOptions : public OptionsTree//, QObject
{
    Q_OBJECT
public:
    static PsiOptions* instance();
    static const PsiOptions* defaults();
    static void reset();

    static bool exists(QString fileName);
    ~PsiOptions();
    bool load(QString file);
    void load(XMPP::Client* client);
    bool newProfile();
    bool save(QString file);
    void autoSave(bool autoSave, QString autoFile = "");
    void resetOption(const QString &name);

// don't call this normally
    PsiOptions();

private slots:
    void saveToAutoFile();
    void getOptionsStorage_finished();

private:
    QString autoFile_;
    QTimer *autoSaveTimer_;
    static PsiOptions* instance_;
    static PsiOptions* defaults_;
};

#endif // PSIOPTIONS_H
