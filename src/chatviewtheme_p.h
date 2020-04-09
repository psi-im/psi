/*
 * chatviewtheme_p.h
 * Copyright (C) 2017  Sergey Ilinykh
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

#ifndef CHATVIEWTHEME_P_H
#define CHATVIEWTHEME_P_H

#include "chatviewtheme.h"
#include "theme_p.h"

#include <QPointer>
#include <QScopedPointer>
#include <QTimer>
#ifdef WEBENGINE
#include <QWebEngineScript>
#else
#endif

class ChatViewThemePrivate;
class ChatViewThemeProvider;
class NetworkAccessManager;
class QWebFrame;
class WebView;

class ChatViewJSLoader : public QObject {
    Q_OBJECT

    ChatViewThemePrivate *    theme;
    QString                   _loadError;
    QHash<QString, QObject *> _sessions;

    Q_PROPERTY(QString themeId READ themeId CONSTANT)
    Q_PROPERTY(QString isMuc READ isMuc CONSTANT)
    Q_PROPERTY(QString serverUrl READ serverUrl CONSTANT)

signals:
    void sessionHtmlReady(const QString &sessionId, const QString &html);

public:
    ChatViewJSLoader(ChatViewThemePrivate *theme, QObject *parent = nullptr);
    const QString themeId() const;
    bool          isMuc() const;
    QString       serverUrl() const;
    void          registerSession(ChatViewThemeSession *session);
    void          unregisterSession(const QString &sessId);

private slots:
    void _callFinishLoadCalbacks();

public slots:
    void setMetaData(const QVariantMap &map);
    void finishThemeLoading();
    void errorThemeLoading(const QString &error);
    void setHtml(const QString &h);
    void setHttpResourcePath(const QString &relPath);

    // we don't need not text cache since binary data(images?)
    // most likely will be cached by webkit itself
    void toCache(const QString &name, const QVariant &data);

    /**
     * @brief loads content to cache
     * @param map(cache_key => file in theme)
     */
    void saveFilesToCache(const QVariantMap &map);

    /**
     * @brief That's about applying theme to certian session. So we register session's id in theme and allow theme
     *        loader in theme's webview to init last parts of theme.
     *        This is for cases when theme can't init itself fully w/o some knowledge about sesion.
     * @param sessionId it's the same id as registered on internal web server
     * @param props a list of proprties' names
     * @return filled map prop=>value
     */
    QVariantMap sessionProperties(const QString &sessionId, const QVariantList &props);
    void        setCaseInsensitiveFS(bool state = true);
    void        setPrepareSessionHtml(bool enabled = true);
    void        setSessionHtml(const QString &sessionId, const QString &html);
    QVariantMap checkFilesExist(const QStringList &files, const QString baseDir = QString());
    QString     getFileContents(const QString &name) const;
    QString     getFileContentsFromAdapterDir(const QString &name) const;
    void        setTransparent();
};

// JS Bridge object emedded by theme. Has any logic unrelted to contact itself
class ChatViewThemeJSUtil : public QObject {
    Q_OBJECT

    ChatViewThemePrivate *theme;
    QString               psiDefaultAvatarUrl;
    QStringList           changedOptions;
    QTimer                optChangeTimer;

    Q_PROPERTY(QString psiDefaultAvatarUrl MEMBER psiDefaultAvatarUrl CONSTANT)

signals:
    void optionsChanged(const QStringList &);

public:
    ChatViewThemeJSUtil(ChatViewThemePrivate *theme, QObject *parent = nullptr);
    void putToCache(const QString &key, const QVariant &data);

public slots:
    QVariantMap loadFromCacheMulti(const QVariantList &list);
    QVariant    cache(const QString &name) const;
    QString     psiOption(const QString &option) const;
    QString     psiOptions(const QStringList &options) const;
    QString     colorOption(const QString &option) const;
    QString     formatDate(const QDateTime &dt, const QString &format) const;
    QString     strftime(const QDateTime &dt, const QString &format) const;
    void        console(const QString &text) const;
    QString     status2text(int status) const;
    QString     hex2rgba(const QString &hex, float opacity);
private slots:
    void sendOptionsChanges();
    void optionsChanged(const QString &option);
};

class ChatViewThemePrivate : public ThemePrivate {
public:
    QString                             html;
    QString                             httpRelPath;
    QScopedPointer<ChatViewJSLoader>    jsLoader;
    QScopedPointer<ChatViewThemeJSUtil> jsUtil; // it's abslutely the same object for every theme.
    QPointer<WebView>                   wv;
    QMap<QString, QVariant>             cache;
    bool                           prepareSessionHtml    = false; // if html should be generated by JS for each session.
    bool                           transparentBackground = false;
    QPointer<NetworkAccessManager> nam;

#ifdef WEBENGINE
    QList<QWebEngineScript> scripts;
#else
    QStringList scripts;
#endif
    QList<std::function<void(bool)>> loadCallbacks;

#ifndef WEBENGINE
    QVariant evaluateFromFile(const QString fileName, QWebFrame *frame);
#endif

    friend class ChatViewThemeJSUtil;

    ChatViewThemePrivate(ChatViewThemeProvider *provider);
    ~ChatViewThemePrivate();

    bool     exists();
    bool     load(std::function<void(bool)> loadCallback);
    bool     hasPreview() const;
    QWidget *previewWidget();

    bool isMuc() const;

    bool isTransparentBackground() const;
#ifndef WEBENGINE
    void embedSessionJsObject(ChatViewThemeSession *session);
#endif
    bool applyToSession(ChatViewThemeSession *session);

    QVariantMap loadFromCacheMulti(const QVariantList &list);
};

#endif // CHATVIEWTHEME_P_H
