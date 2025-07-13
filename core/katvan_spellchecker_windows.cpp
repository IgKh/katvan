/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2025 Igor Khanin
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

#include <QDebug>

#include <objidl.h>
#include <spellcheck.h>

namespace katvan {

WindowsSpellChecker::WindowsSpellChecker(QObject* parent)
    : SpellChecker(parent)
    , d_factory(nullptr)
    , d_checker(nullptr)
{
    HRESULT hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d_factory));
    if (FAILED(hr)) {
        qWarning() << "Failed to create SpellCheckerFactory:" << hr;
    }
}

WindowsSpellChecker::~WindowsSpellChecker()
{
    if (d_checker != nullptr) {
        d_checker->Release();
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

void WindowsSpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictPath)
{
    if (d_factory == nullptr) {
        return;
    }

    if (d_checker != nullptr) {
        d_checker->Release();
        d_checker = nullptr;
    }

    if (dictName.isEmpty()) {
        SpellChecker::setCurrentDictionary(dictName, dictPath);
        return;
    }
    std::wstring lang = dictName.toStdWString();

    HRESULT hr = d_factory->CreateSpellChecker(lang.data(), &d_checker);
    if (FAILED(hr)) {
        qWarning() << "Failed to create spell checker for" << dictName << ":" << hr;
        return;
    }

    SpellChecker::setCurrentDictionary(dictName, dictPath);
}

SpellChecker::MisspelledWordRanges WindowsSpellChecker::checkSpelling(const QString& text)
{
    SpellChecker::MisspelledWordRanges result;
    if (d_checker == nullptr) {
        return result;
    }

    std::wstring str = text.toStdWString();

    IEnumSpellingError* errors = nullptr;
    HRESULT hr = d_checker->ComprehensiveCheck(str.data(), &errors);
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

            result.append(std::make_pair(start, length));
        }
    }
    errors->Release();

    return result;
}

void WindowsSpellChecker::addToPersonalDictionary(const QString& word)
{
    if (d_checker == nullptr) {
        return;
    }

    std::wstring str = word.toStdWString();

    HRESULT hr = d_checker->Add(str.data());
    if (FAILED(hr)) {
        qWarning() << "SpellChecker::Add failed for" << word << ":" << hr;
    }
}

void WindowsSpellChecker::requestSuggestionsImpl(const QString& word, int position)
{
    if (d_checker == nullptr) {
        return;
    }

    QStringList result;
    std::wstring str = word.toStdWString();

    IEnumString* suggestions = nullptr;
    HRESULT hr = d_checker->Suggest(str.data(), &suggestions);
    if (FAILED(hr)) {
        qWarning() << "SpellChecker::Suggest failed for" << word << ":" << hr;
        return;
    }

    hr = S_OK;
    while (hr == S_OK) {
        LPOLESTR suggestion = nullptr;
        hr = suggestions->Next(1, &suggestion, nullptr);

        if (hr == S_OK) {
            result.append(QString::fromWCharArray(suggestion));
            CoTaskMemFree(suggestion);
        }
    }

    suggestions->Release();
    suggestionsCalculated(word, position, result);
}

}

#include "moc_katvan_spellchecker_windows.cpp"
