/*
 * theme.h - base class for any theme
 * Copyright (C) 2010-2017  Justin Karneges, Michail Pishchagin, Sergey Ilinykh
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

/*
 MOST IMPORTANT RULE: the theme never depends on thing to which its applied,
 so its one-way relation.
*/

#ifndef THEME_H
#define THEME_H

#include <QHash>
#include <QObject>
#include <QSharedData>
#include <QStringList>
#include <functional>

class PsiThemeProvider;
class QFileInfo;
class QWidget;
class ThemePrivate;

//-----------------------------------------------
// Theme
//-----------------------------------------------
class Theme {
public:
    class ResourceLoader {
        // By default theme does not internal info about its fs.
        // That means rereading zip themes on each file or
        // dir by dir search of each file with name in invalid case.
        // To optimize it, we have this class. It keeps zip opened
        // has a cache of mapping requested names to real.

    public:
        virtual ~ResourceLoader();
        virtual QByteArray loadData(const QString &fileName)   = 0;
        virtual bool       fileExists(const QString &fileName) = 0;
    };

    enum class State : char { Invalid, NotLoaded, Loading, Loaded };

    Theme();
    Theme(ThemePrivate *priv);
    Theme(const Theme &other);
    Theme &operator=(const Theme &other);
    virtual ~Theme();

    bool  isValid() const;
    State state() const;

    // previously virtual
    bool exists();
    bool load();                                       // synchronous load
    bool load(std::function<void(bool)> loadCallback); // asynchronous load

    bool     hasPreview() const;
    QWidget *previewWidget(); // this hack must be replaced with something widget based
    // end of previously virtual

    static bool isCompressed(const QFileInfo &); // just tells if theme looks like compressed.
    bool        isCompressed() const;
    // load file from theme in `themePath`
    static QByteArray loadData(const QString &fileName, const QString &themePath, bool caseInsensetive = false,
                               bool *loaded = nullptr);
    QByteArray        loadData(const QString &fileName, bool *loaded = nullptr) const;
    ResourceLoader *  resourceLoader() const;

    const QString      id() const;
    void               setId(const QString &id);
    const QString &    name() const;
    void               setName(const QString &name);
    const QString &    version() const;
    const QString &    description() const;
    const QStringList &authors() const;
    const QString &    creation() const;
    const QString &    homeUrl() const;

    PsiThemeProvider *            themeProvider() const;
    const QString &               filePath() const;
    void                          setFilePath(const QString &f);
    const QHash<QString, QString> info() const;
    void                          setInfo(const QHash<QString, QString> &i);

    void setCaseInsensitiveFS(bool state);
    bool caseInsensitiveFS() const;

    QString title() const; // helper function to remove name or id when name is not set

    // for internal use
    template <class T> T *priv() const { return static_cast<T *>(d.data()); }
    void                  setState(State state);

private:
    friend class ThemePrivate;
    QExplicitlySharedDataPointer<ThemePrivate> d;
};

#endif // THEME_H
