/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "qsourcehighliter.h"
#include <QString>
#include <algorithm>
#include <optional>

// Shared by the rich-text highlighter and the embedded code-block widget.
namespace PsiCodeLanguage {

using SourceLanguage = QSourceHighlite::QSourceHighliter::Language;

inline std::optional<SourceLanguage> fromName(QString language)
{
    language = language.trimmed().toLower();
    const int whitespace = [&language] {
        const int space = language.indexOf(QLatin1Char(' '));
        const int tab = language.indexOf(QLatin1Char('\t'));
        if (space < 0) {
            return tab;
        }
        if (tab < 0) {
            return space;
        }
        return std::min(space, tab);
    }();
    if (whitespace >= 0) {
        language.truncate(whitespace);
    }

    if (language == QLatin1String("cpp") || language == QLatin1String("c++")
        || language == QLatin1String("cxx") || language == QLatin1String("cc")) {
        return SourceLanguage::CodeCpp;
    }
    if (language == QLatin1String("c")) {
        return SourceLanguage::CodeC;
    }
    if (language == QLatin1String("js") || language == QLatin1String("javascript")) {
        return SourceLanguage::CodeJs;
    }
    if (language == QLatin1String("bash") || language == QLatin1String("sh")
        || language == QLatin1String("shell")) {
        return SourceLanguage::CodeBash;
    }
    if (language == QLatin1String("php")) {
        return SourceLanguage::CodePHP;
    }
    if (language == QLatin1String("qml")) {
        return SourceLanguage::CodeQML;
    }
    if (language == QLatin1String("py") || language == QLatin1String("python")) {
        return SourceLanguage::CodePython;
    }
    if (language == QLatin1String("rs") || language == QLatin1String("rust")) {
        return SourceLanguage::CodeRust;
    }
    if (language == QLatin1String("java")) {
        return SourceLanguage::CodeJava;
    }
    if (language == QLatin1String("cs") || language == QLatin1String("c#")
        || language == QLatin1String("csharp")) {
        return SourceLanguage::CodeCSharp;
    }
    if (language == QLatin1String("go")) {
        return SourceLanguage::CodeGo;
    }
    if (language == QLatin1String("v")) {
        return SourceLanguage::CodeV;
    }
    if (language == QLatin1String("sql")) {
        return SourceLanguage::CodeSQL;
    }
    if (language == QLatin1String("json")) {
        return SourceLanguage::CodeJSON;
    }
    if (language == QLatin1String("xml") || language == QLatin1String("html")) {
        return SourceLanguage::CodeXML;
    }
    if (language == QLatin1String("css")) {
        return SourceLanguage::CodeCSS;
    }
    if (language == QLatin1String("ts") || language == QLatin1String("typescript")) {
        return SourceLanguage::CodeTypeScript;
    }
    if (language == QLatin1String("yaml") || language == QLatin1String("yml")) {
        return SourceLanguage::CodeYAML;
    }
    if (language == QLatin1String("ini")) {
        return SourceLanguage::CodeINI;
    }
    if (language == QLatin1String("vex")) {
        return SourceLanguage::CodeVex;
    }
    if (language == QLatin1String("cmake")) {
        return SourceLanguage::CodeCMake;
    }
    if (language == QLatin1String("make") || language == QLatin1String("makefile")) {
        return SourceLanguage::CodeMake;
    }
    if (language == QLatin1String("asm") || language == QLatin1String("assembly")) {
        return SourceLanguage::CodeAsm;
    }
    if (language == QLatin1String("lua")) {
        return SourceLanguage::CodeLua;
    }
    if (language == QLatin1String("rhai")) {
        return SourceLanguage::CodeRhai;
    }

    return std::nullopt;
}

} // namespace PsiCodeLanguage
