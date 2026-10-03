/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2026 Igor Khanin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "katvan_spellchecker_windows.h"
#include "katvan_spellchecker_personal.h"

#include <QDebug>

#include <objidl.h>
#include <spellcheck.h>

namespace katvan {

WindowsSpellChecker::WindowsSpellChecker(QObject* parent)
    : SpellChecker(parent)
    , d_factory(nullptr)
{
    HRESULT hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d_factory));
    if (FAILED(hr)) {
        qWarning() << "Failed to create SpellCheckerFactory:" << hr;
    }

    d_personalDictionary = new PersonalDictionary(this);
}

WindowsSpellChecker::~WindowsSpellChecker()
{
    for (ISpellChecker* checker: d_checkers) {
        checker->Release();
    }

    if (d_factory != nullptr) {
        d_factory->Release();
    }
}

QMap<QString, QString> WindowsSpellChecker::findDictionaries()
{
    QMap<QString, QString> result;

    if (d_factory == nullptr) {
        return result;
    }

    IEnumString* languages = nullptr;
    HRESULT hr = d_factory->get_SupportedLanguages(&languages);
    if (FAILED(hr)) {
        qWarning() << "SpellCheckerFactory::get_SupportedLanguages failed:" << hr;
        return result;
    }

    hr = S_OK;
    while (hr == S_OK) {
        LPOLESTR str = nullptr;
        hr = languages->Next(1, &str, nullptr);

        if (hr == S_OK) {
            QString lang = QString::fromWCharArray(str);
            if (!lang.isEmpty()) {
                result.insert(lang, QString());
            }
            CoTaskMemFree(str);
        }
    }
    languages->Release();

    return result;
}

void WindowsSpellChecker::setCurrentDictionaries(const QList<DictionaryDef>& dicts)
{
    if (d_factory == nullptr) {
        return;
    }

    for (ISpellChecker* checker: d_checkers) {
        checker->Release();
    }
    d_checkers.clear();

    for (const auto& [dictName, dictPath] : dicts) {
        std::wstring lang = dictName.toStdWString();

        ISpellChecker* checker = nullptr;
        HRESULT hr = d_factory->CreateSpellChecker(lang.data(), &checker);
        if (FAILED(hr)) {
            qWarning() << "Failed to create spell checker for" << dictName << ":" << hr;
            continue;
        }
        d_checkers.append(checker);
    }

    SpellChecker::setCurrentDictionaries(dicts);
}

static QString getCheckerId(ISpellChecker* checker)
{
    LPWSTR id = nullptr;
    HRESULT hr = checker->get_Id(&id);
    if (FAILED(hr)) {
        return QString();
    }

    QString result = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return result;
}

static bool isValidWord(ISpellChecker* checker, const std::wstring& word)
{
    IEnumSpellingError* errors = nullptr;
    HRESULT hr = checker->Check(word.data(), &errors);
    if (FAILED(hr)) {
        qWarning() << "Failed check in checker" << getCheckerId(checker) << ":" << hr;
        return false;
    }

    ISpellingError* error = nullptr;
    bool result = (errors->Next(&error) != S_OK);
    if (error) {
        error->Release();
    }
    errors->Release();
    return result;
}

SpellChecker::MisspelledWordRanges WindowsSpellChecker::checkSpelling(const QString& text)
{
    SpellChecker::MisspelledWordRanges result;
    if (d_checkers.isEmpty()) {
        return result;
    }

    std::wstring str = text.toStdWString();

    // Different language pack checkers can tokenize differently, so we'll have
    // the first checker tokenize, and then for every error it finds double check
    // to see if any other checker accepts it.
    ISpellChecker* checker = d_checkers.first();

    IEnumSpellingError* errors = nullptr;
    HRESULT hr = checker->ComprehensiveCheck(str.data(), &errors);
    if (FAILED(hr)) {
        qWarning() << "Failed comprehensive check:" << hr;
        return result;
    }

    hr = S_OK;
    while (hr == S_OK) {
        ISpellingError* error = nullptr;

        hr = errors->Next(&error);
        if (hr == S_OK) {
            ULONG start = 0, length = 0;
            error->get_StartIndex(&start);
            error->get_Length(&length);
            error->Release();

            std::wstring word = str.substr(start, length);
            bool skip = false;

            for (qsizetype j = 1; j < d_checkers.size(); j++) {
                if (isValidWord(d_checkers[j], word)) {
                    skip = true;
                    break;
                }
            }

            if (!skip && d_personalDictionary->isWordInDictionary(QString::fromStdWString(word))) {
                skip = true;
            }

            if (!skip) {
                result.append(std::make_pair(start, length));
            }
        }
    }
    errors->Release();

    return result;
}

bool WindowsSpellChecker::addToPersonalDictionary(const QString& word)
{
    d_personalDictionary->addToDictionary(word);
    return true;
}

void WindowsSpellChecker::requestSuggestionsImpl(const QString& word, int position)
{
    if (d_checkers.isEmpty()) {
        return;
    }

    QStringList result;
    std::wstring str = word.toStdWString();

    for (ISpellChecker* checker : d_checkers) {
        if (isValidWord(checker, str)) {
            continue;
        }

        IEnumString* suggestions = nullptr;
        HRESULT hr = checker->Suggest(str.data(), &suggestions);
        if (FAILED(hr)) {
            qWarning() << "SpellChecker::Suggest failed for" << word << "in" << getCheckerId(checker) << ":" << hr;
            continue;
        }

        hr = S_OK;
        while (hr == S_OK) {
            LPOLESTR suggestion = nullptr;
            hr = suggestions->Next(1, &suggestion, nullptr);

            if (hr == S_OK) {
                QString candidate = QString::fromWCharArray(suggestion);
                if (!result.contains(candidate)) {
                    result.append(candidate);
                }

                CoTaskMemFree(suggestion);
            }
        }

        suggestions->Release();
    }

    suggestionsCalculated(word, position, result);
}

}

#include "moc_katvan_spellchecker_windows.cpp"
