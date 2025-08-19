#include "textutil.h"

#include "coloropt.h"
#include "emojiregistry.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "rtparse.h"

#include <QTextDocument> // for escape()

QString TextUtil::escape(const QString &plain) { return plain.toHtmlEscaped(); }

/**
 * Converts a given QString containing escaped XML/HTML entities into their unescaped equivalents.
 *
 * This function replaces specific XML/HTML escape sequences within the input string
 * with their corresponding special characters. The replacements include:
 * - "&lt;" with "<"
 * - "&gt;" with ">"
 * - "&quot;" with "\""
 * - "&amp;" with "&"
 *
 * @param escaped The QString containing escaped XML/HTML entities to be unescaped.
 * @return A QString where the escape sequences are replaced with their corresponding characters.
 */
QString TextUtil::unescape(const QString &escaped)
{
    QString plain = escaped;
    plain.replace("&lt;", "<");
    plain.replace("&gt;", ">");
    plain.replace("&quot;", "\"");
    plain.replace("&amp;", "&");
    return plain;
}

/**
 * Quote text for a response with "> " prefix.
 * Nested quotes and word wrapping are supported.
 * assert(TextUtil::quote("Hello world", 80, true) == "> Hello world\n\n")
 * // Line wrapping
 * assert(TextUtil::quote("This is a very long line that should be wrapped", 20, true) == "> This is a very\n> long line that should be\n> wrapped\n\n")
 * // Empty lines
 * assert(TextUtil::quote("Line1\n\nLine2", 80, false) == "> Line1\n\n> Line2\n\n")
 * // Empty lines: quoteEmpty
 * assert(TextUtil::quote("Line1\n\nLine2", 80, true) == "> Line1\n>\n> Line2\n\n")
 * @param toquote text to quote
 * @param width width where to wrap text
 * @param quoteEmpty add > to empty lines
 * @return
 */
QString TextUtil::quote(const QString &toquote, int width, bool quoteEmpty)
{
    int quoteLevel = 0; // amount of leading '>' in the current line
    int column = 0; // current column
    bool atLineStart = true; // at beginning of line
    int lastSpaceIndex = 0; // index of last whitespace to break line

    static const QRegularExpression rxTrimTrailingSpaces(QStringLiteral(" +\n"));
    static const QRegularExpression rxUnquote1(QStringLiteral("^>+\n"));
    static const QRegularExpression rxUnquote2(QStringLiteral("\n>+\n"));
    static const QRegularExpression rxFollowLineEmpty(QStringLiteral("\n"));
    static const QRegularExpression rxFollowLinePattern(QStringLiteral("\n(?!\\s*\n)"));
    static const QRegularExpression rxCompress(QStringLiteral("> +>"));

    // quote first line
    QString            quoted = QStringLiteral("> ") + toquote;
    QRegularExpression rx = quoteEmpty ? rxFollowLineEmpty : rxFollowLinePattern;
    // quote the following lines
    quoted.replace(rx, QStringLiteral("\n> "));
    // compress > > > > quotes to >>>>
    quoted.replace(rxCompress, QStringLiteral(">>"));
    quoted.replace(rxCompress, QStringLiteral(">>"));
    // remove trailing spaces before a newline
    quoted.replace(rxTrimTrailingSpaces, QStringLiteral("\n"));

    if (!quoteEmpty) {
        // unquote empty lines
        quoted.replace(rxUnquote1, QStringLiteral("\n\n"));
        quoted.replace(rxUnquote2, QStringLiteral("\n\n"));
    }

    for (int i = 0; i < int(quoted.length()); i++) {
        column++;
        if (atLineStart && quoted[i] == '>') {
            quoteLevel++;
        } else {
            atLineStart = false;
        }

        switch (quoted[i].toLatin1()) {
        case '\n':
            // Reset state at a newline
            quoteLevel  = 0;
            column      = 0;
            atLineStart = true;
            break;
        case ' ':
        case '\t':
            lastSpaceIndex = i;
            break;
        }
        if (quoted[i] == '\n') {
            quoteLevel = 0;
            atLineStart = true;
        }

        if (column > width) {
            // If we have no breakable space in range, advance to the next whitespace
            if ((lastSpaceIndex + width) < i) {
                // advance to the next whitespace from the position
                lastSpaceIndex = i;
                i  = quoted.length();
                while ((lastSpaceIndex < i) && !quoted[lastSpaceIndex].isSpace()) {
                    lastSpaceIndex++;
                }
                i = lastSpaceIndex;
            }
            if ((i < int(quoted.length())) && (quoted[lastSpaceIndex] != '\n')) {
                // Insert newline at lastSpaceIndex
                quoted.insert(lastSpaceIndex, QLatin1Char('\n'));
                ++lastSpaceIndex;
                // Re-insert the current quote prefix to the next line
                quoted.insert(lastSpaceIndex, QString().fill(QLatin1Char('>'), quoteLevel));
                // count of inserted '>' chars
                i += (quoteLevel + 1);
                // Reset wrapping state for the new line
                column = 0;
            }
        }
    }
    // add two empty lines to the quoted text - the cursor will be positioned at the end of those.
    quoted += QStringLiteral("\n\n");
    return quoted;
}

QString TextUtil::plain2rich(const QString &plain)
{
    QString rich;

    for (int i = 0; i < int(plain.length()); ++i) {
#ifdef Q_OS_WIN
        if (plain[i] == '\r' && i + 1 < (int)plain.length() && plain[i + 1] == '\n')
            ++i; // Qt/Win sees \r\n as two new line chars
#endif
        if (plain[i] == '\n') {
            rich += "<br>";
        } else if (plain[i] == ' ' && !rich.isEmpty() && rich[rich.size() - 1] == ' ')
            rich += "&nbsp;"; // instead of pre-wrap, which prewraps \n as well
        else if (plain[i] == '\t')
            rich += "&nbsp; &nbsp; &nbsp; ";
        else if (plain[i] == '<')
            rich += "&lt;";
        else if (plain[i] == '>')
            rich += "&gt;";
        else if (plain[i] == '\"')
            rich += "&quot;";
        else if (plain[i] == '\'')
            rich += "&apos;";
        else if (plain[i] == '&')
            rich += "&amp;";
        else
            rich += plain[i];
    }

    return rich;
    // return "<span style='white-space: pre-wrap'>" + rich + "</span>";
}

QString TextUtil::rich2plain(const QString &in, bool collapseSpaces)
{
    QString out;

    for (int i = 0; i < int(in.length()); ++i) {
        // tag?
        if (in[i] == '<') {
            // find end of tag
            ++i;
            int n = in.indexOf('>', i);
            if (n == -1)
                break;
            QString str = in.mid(i, (n - i));
            i           = n;

            QString tagName;
            n = str.indexOf(' ');
            if (n != -1)
                tagName = str.mid(0, n);
            else
                tagName = str;

            if (tagName == "br")
                out += '\n';

            // handle output of Qt::convertFromPlainText() correctly
            if ((tagName == "p" || tagName == "/p" || tagName == "div") && out.length() > 0)
                out += '\n';
        }
        // entity?
        else if (in[i] == '&') {
            // find a semicolon
            ++i;
            int n = in.indexOf(';', i);
            if (n == -1)
                break;
            QString type = in.mid(i, (n - i));
            i            = n; // should be n+1, but we'll let the loop increment do it

            if (type == "amp")
                out += '&';
            else if (type == "lt")
                out += '<';
            else if (type == "gt")
                out += '>';
            else if (type == "quot")
                out += '\"';
            else if (type == "apos")
                out += '\'';
        } else if (in[i].isSpace()) {
            if (in[i] == QChar::Nbsp)
                out += ' ';
            else if (in[i] != '\n') {
                if (!collapseSpaces || i == 0 || out.length() == 0)
                    out += ' ';
                else {
                    QChar last = out.at(out.length() - 1);
                    bool  ok   = true;
                    if (last.isSpace() && last != '\n')
                        ok = false;
                    if (ok)
                        out += ' ';
                }
            }
        } else {
            out += in[i];
        }
    }

    return out;
}

QString TextUtil::resolveEntities(const QStringView &in)
{
    QString out;

    for (int i = 0; i < int(in.length()); ++i) {
        if (in[i] == '&') {
            // find a semicolon
            ++i;
            int n = in.indexOf(';', i);
            if (n == -1)
                break;
            auto type = in.mid(i, (n - i));

            i = n; // should be n+1, but we'll let the loop increment do it

            if (type == QLatin1String { "amp" })
                out += '&';
            else if (type == QLatin1String { "lt" })
                out += '<';
            else if (type == QLatin1String { "gt" })
                out += '>';
            else if (type == QLatin1String { "quot" })
                out += '\"';
            else if (type == QLatin1String { "apos" })
                out += '\'';
            else if (type == QLatin1String { "nbsp" })
                out += char(0xa0);
        } else {
            out += in[i];
        }
    }

    return out;
}

static bool linkify_pmatch(const QString &str1, int at, const QString &str2)
{
    if (str2.length() > (str1.length() - at))
        return false;

    for (int n = 0; n < int(str2.length()); ++n) {
        if (str1.at(n + at).toLower() != str2.at(n).toLower())
            return false;
    }

    return true;
}

static bool linkify_isOneOf(const QChar &c, const QString &charlist)
{
    for (int i = 0; i < int(charlist.length()); ++i) {
        if (c == charlist.at(i))
            return true;
    }

    return false;
}

// encodes a few dangerous html characters
static QString linkify_htmlsafe(const QString &in)
{
    QString out;

    for (int n = 0; n < in.length(); ++n) {
        if (linkify_isOneOf(in.at(n), "\"\'`<>")) {
            // hex encode
            QString hex = QString::asprintf("%%%02X", in.at(n).toLatin1());
            out.append(hex);
        } else {
            out.append(in.at(n));
        }
    }

    return out;
}

static bool linkify_okUrl(const QString &url) { return !(url.at(url.length() - 1) == '.'); }

static bool linkify_okEmail(const QString &addy)
{
    // this makes sure that there is an '@' and a '.' after it, and that there is
    // at least one char for each of the three sections
    int n = addy.indexOf('@');
    if (n == -1 || n == 0)
        return false;
    int d = addy.indexOf('.', n + 1);
    if (d == -1 || d == 0)
        return false;
    if ((addy.length() - 1) - d <= 0)
        return false;
    return addy.indexOf("..") == -1;
}

static void emojiconifyPlainText(RTParse &p, const QString &in)
{
    const auto &reg            = EmojiRegistry::instance();
    int         idx            = 0;
    int         emojisStartIdx = -1;
    QStringView ref;

    auto dump_emoji = [&p, &emojisStartIdx, &in, &idx]() {
        auto code = QStringView { in }.mid(emojisStartIdx, idx - emojisStartIdx).toString();
#if defined(WEBKIT) || defined(WEBENGINE)
        p.putRich(QLatin1String(R"html(<span class="emojis">)html") + code + QLatin1String("</span>"));
#else
        // FIXME custom style here is a hack. This supposed to be handled via style resource in PsiTextView
        p.putRich(
            QString(
                R"(<icon text="%1" min-height="1.25em" max-height="1.7em" font-size="1.3em" valign="bottom" type="smiley">)")
                .arg(code));
#endif
    };
    int position;
    while (std::tie(ref, position) = reg.findEmoji(in, idx), !ref.isEmpty()) {
        if (emojisStartIdx == -1) {
            emojisStartIdx = position;
            p.putPlain(in.left(position));
        } else if (position != idx) { // a text gap
            dump_emoji();
            emojisStartIdx = position;
            p.putPlain(in.mid(idx, position - idx));
        }
        idx = position + ref.size();
    }
    if (emojisStartIdx == -1)
        p.putPlain(in);
    else {
        dump_emoji();
        p.putPlain(in.right(in.size() - idx));
    }
}

/**
 * takes a richtext string and heuristically adds links for uris of common protocols
 * @return a richtext string with link markup added
 */
QString TextUtil::linkify(const QString &in)
{
    QString out = in;
    int     x1, x2;
    bool    isUrl, isAtStyle;
    QString linked, link, href;

    for (int n = 0; n < int(out.length()); ++n) {
        isUrl     = false;
        isAtStyle = false;
        x1        = n;

        if (linkify_pmatch(out, n, "xmpp:")) {
            n += 5;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "mailto:")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "http://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "https://")) {
            n += 8;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "git://")) {
            n += 6;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "ftp://")) {
            n += 6;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "ftps://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "sftp://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "news://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "ed2k://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "file://")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "magnet:")) {
            n += 7;
            isUrl = true;
            href  = "";
        } else if (linkify_pmatch(out, n, "www.")) {
            isUrl = true;
            href  = "https://";
        } else if (linkify_pmatch(out, n, "ftp.")) {
            isUrl = true;
            href  = "ftp://";
        } else if (linkify_pmatch(out, n, "@")) {
            isAtStyle = true;
            href      = "x-psi-atstyle:";
        }

        if (isUrl) {
            // make sure the previous char is not alphanumeric
            if (x1 > 0 && out.at(x1 - 1).isLetterOrNumber())
                continue;

            // find whitespace (or end)
            QMap<QChar, int> brackets;
            brackets['('] = brackets[')'] = brackets['['] = brackets[']'] = brackets['{'] = brackets['}'] = 0;
            QMap<QChar, QChar> openingBracket;
            openingBracket[')'] = '(';
            openingBracket[']'] = '[';
            openingBracket['}'] = '{';
            for (x2 = n; x2 < int(out.length()); ++x2) {
                if (out.at(x2).isSpace() || linkify_isOneOf(out.at(x2), "\"\'`<>") || linkify_pmatch(out, x2, "&quot;")
                    || linkify_pmatch(out, x2, "&apos;") || linkify_pmatch(out, x2, "&gt;")
                    || linkify_pmatch(out, x2, "&lt;")) {
                    break;
                }
                if (brackets.contains(out.at(x2))) {
                    ++brackets[out.at(x2)];
                }
            }
            int     len = x2 - x1;
            QString pre = out.mid(x1, x2 - x1);
            pre         = resolveEntities(pre);

            // go backward hacking off unwanted punctuation
            int cutoff;
            for (cutoff = pre.length() - 1; cutoff >= 0; --cutoff) {
                if (!linkify_isOneOf(pre.at(cutoff), "!?,.()[]{}<>\""))
                    break;
                if (linkify_isOneOf(pre.at(cutoff), ")]}")
                    && brackets[pre.at(cutoff)] - brackets[openingBracket[pre.at(cutoff)]] <= 0) {
                    break; // in theory, there could be == above, but these are urls, not math ;)
                }
                if (brackets.contains(pre.at(cutoff))) {
                    --brackets[pre.at(cutoff)];
                }
            }
            ++cutoff;
            //++x2;

            link = pre.mid(0, cutoff);
            if (!linkify_okUrl(link)) {
                n = x1 + link.length();
                continue;
            }
            href += link;
            // attributes need to be encoded too.
            href = escape(href);
            href = linkify_htmlsafe(href);
            // printf("link: [%s], href=[%s]\n", link.latin1(), href.latin1());
#ifdef WEBKIT
            linked = QString("<a href=\"%1\">").arg(href);
#else
            auto linkColor = ColorOpt::instance()->color("options.ui.look.colors.messages.link");
            // we have visited link as well but it's no applicable to QTextEdit or we have to track visited manually
            linked = QString("<a href=\"%1\" style=\"color:%2\">").arg(href, linkColor.name());
#endif
            linked += (escape(link) + "</a>" + escape(pre.mid(cutoff)));
            out.replace(x1, len, linked);
            n = x1 + linked.length() - 1;
        } else if (isAtStyle) {
            // go backward till we find the beginning
            if (x1 == 0)
                continue;
            --x1;
            for (; x1 >= 0; --x1) {
                if (!linkify_isOneOf(out.at(x1), "_.-+") && !out.at(x1).isLetterOrNumber())
                    break;
            }
            ++x1;

            // go forward till we find the end
            x2 = n + 1;
            for (; x2 < int(out.length()); ++x2) {
                if (!linkify_isOneOf(out.at(x2), "_.-+") && !out.at(x2).isLetterOrNumber())
                    break;
            }

            int len = x2 - x1;
            link    = out.mid(x1, len);
            // link = resolveEntities(link);

            if (!linkify_okEmail(link)) {
                n = x1 + link.length();
                continue;
            }

            href += link;
            // printf("link: [%s], href=[%s]\n", link.latin1(), href.latin1());
            linked = QString("<a href=\"%1\">").arg(href) + link + "</a>";
            out.replace(x1, len, linked);
            n = x1 + linked.length() - 1;
        }
    }

    return out;
}

// sickening
QString TextUtil::emoticonify(const QString &in)
{
    RTParse p(in);
    while (!p.atEnd()) {
        // returns us the first chunk as a plaintext string
        QString str = p.next();

        int i = 0;
        while (i >= 0) {
            // find closest emoticon
            int      ePos    = -1;
            PsiIcon *closest = nullptr;

            int foundPos = -1, foundLen = -1;

            for (const Iconset &iconset : std::as_const(PsiIconset::instance()->emoticons)) {
                QListIterator<PsiIcon *> it = iconset.iterator();
                while (it.hasNext()) {
                    PsiIcon *icon = it.next();
                    if (icon->regExp().pattern().isEmpty())
                        continue;

                    // some hackery
                    int  iii = i;
                    bool searchAgain;

                    // search the first occurance of the emoticon in the remaining text
                    // which is surronded with at least one space
                    do {
                        searchAgain = false;

                        // find the closest match
                        const QRegularExpression &rx    = icon->regExp();
                        auto                      match = rx.match(str, iii);

                        if (!match.hasMatch())
                            continue;

                        int n = match.capturedStart();
                        if (ePos == -1 || n < ePos || (match.capturedLength() > foundLen && n < ePos + foundLen)) {
                            bool leftSpace  = n == 0 || (n > 0 && str[n - 1].isSpace());
                            bool rightSpace = (n + match.capturedLength() == int(str.length()))
                                || (n + match.capturedLength() < int(str.length())
                                    && str[n + match.capturedLength()].isSpace());
                            // there must be whitespace at least on one side of the emoticon
                            if (leftSpace || rightSpace || EmojiRegistry::instance().isEmoji(match.captured())) {
                                ePos    = n;
                                closest = icon;

                                foundPos = n;
                                foundLen = match.capturedLength();
                                break;
                            }

                            searchAgain = true;
                        }

                        iii = n + match.capturedLength();
                    } while (searchAgain);
                }
            }

            QString s;
            if (ePos == -1)
                s = str.mid(i);
            else
                s = str.mid(i, ePos - i);
            emojiconifyPlainText(p, s);
            // p.putPlain(s);

            if (!closest)
                break;

            p.putRich(
                QString(
                    R"(<icon name="%1" text="%2" min-height="1.25em" max-height="1.7em" valign="bottom" type="smiley">)")
                    .arg(TextUtil::escape(closest->name()), TextUtil::escape(str.mid(foundPos, foundLen))));
            i = foundPos + foundLen;
        }
    }

    QString out = p.output();
    return out;
}

QString TextUtil::img2title(const QString &in)
{
    QString            ret = in;
    QRegularExpression rxq("<img[^>]+title\\s*=\\s*'([^']+)'[^>]*>"), rxdq("<img[^>]+title\\s*=\\s*\"([^\"]+)\"[^>]*>");
    ret.replace(rxq, "\\1");
    ret.replace(rxdq, "\\1");
    return ret;
}

QString TextUtil::legacyFormat(const QString &in)
{

    // enable *bold* stuff
    // //old code
    // out=out.replace(QRegularExpression("(^[^<>\\s]*|\\s[^<>\\s]*)\\*(\\S+)\\*([^<>\\s]*\\s|[^<>\\s]*$)"),"\\1<b>*\\2*</b>\\3");
    // out=out.replace(QRegularExpression("(^[^<>\\s\\/]*|\\s[^<>\\s\\/]*)\\/([^\\/\\s]+)\\/([^<>\\s\\/]*\\s|[^<>\\s\\/]*$)"),"\\1<i>/\\2/</i>\\3");
    // out=out.replace(QRegularExpression("(^[^<>\\s]*|\\s[^<>\\s]*)_(\\S+)_([^<>\\s]*\\s|[^<>\\s]*$)"),"\\1<u>_\\2_</u>\\3");

    QString out = in;
    out = out.replace(QRegularExpression("(^|\\s|>)_(\\S+)_(?=<|\\s|$)"), "\\1<u>_\\2_</u>"); // underline inside _text_
    out = out.replace(QRegularExpression("(^|\\s|>)\\*(\\S+)\\*(?=<|\\s|$)"), "\\1<b>*\\2*</b>"); // bold *text*
    out = out.replace(QRegularExpression("(^|\\s|>)\\/(\\S+)\\/(?=<|\\s|$)"), "\\1<i>/\\2/</i>"); // italic /text/

    return out;
}

QString TextUtil::sizeUnit(qlonglong n, qlonglong *div)
{
    qlonglong gb = 1024 * 1024 * 1024;
    qlonglong mb = 1024 * 1024;
    qlonglong kb = 1024;
    QString   unit;
    qlonglong d;
    if (n >= gb) {
        d    = gb;
        unit = QString("GB");
    } else if (n >= mb) {
        d    = mb;
        unit = QString("MB");
    } else if (n >= kb) {
        d    = kb;
        unit = QString("KB");
    } else {
        d    = 1;
        unit = QString("B");
    }

    if (div)
        *div = d;

    return unit;
}

QString TextUtil::roundedNumber(qlonglong n, qlonglong div)
{
    bool decimal = false;
    if (div >= 1024) {
        div /= 10;
        decimal = true;
    }
    qlonglong x_long = n / div;
    int       x      = int(x_long);
    if (decimal) {
        double f = double(x);
        f /= 10;
        return QString::number(f, 'f', 1);
    } else
        return QString::number(x);
}
