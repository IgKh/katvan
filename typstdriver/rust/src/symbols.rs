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
use codex::{Def, Module, Symbol};
use unicode_math_class::MathClass;

pub fn symbols_json() -> anyhow::Result<String> {
    let mut output = vec![];

    collect_symbols(&codex::SYM, "sym", &mut output);
    collect_symbols(&codex::EMOJI, "emoji", &mut output);

    let json = serde_json::to_string(&output)?;
    Ok(json)
}

#[derive(Debug, serde::Serialize)]
struct SymbolEntry {
    pub value: String,
    pub name: String,
    pub description: String,
    pub category: String,
}

impl SymbolEntry {
    fn new(value: &str, name: &str) -> Self {
        let name = name.trim_matches('.').to_owned();
        let codepoint = value.chars().next();

        let description = codepoint
            .and_then(|c| unicode_names2::name(c))
            .map(Iterator::collect)
            .unwrap_or_default();

        let category = if name.starts_with("emoji.") {
            String::from("Emoji")
        } else {
            codepoint
                .and_then(|c| unicode_math_class::class(c))
                .map(math_class_name)
                .map(String::from)
                .unwrap_or_default()
        };

        Self {
            value: String::from(value),
            name,
            description,
            category,
        }
    }
}

fn collect_symbols(module: &Module, module_name: &str, output: &mut Vec<SymbolEntry>) {
    for (name, binding) in module.iter() {
        let item_name = format!("{module_name}.{name}");
        match binding.def {
            Def::Symbol(sym) => match sym {
                Symbol::Single(value) => {
                    output.push(SymbolEntry::new(value, &item_name));
                }
                Symbol::Multi(chars) => {
                    for (modifiers, value, _deprecation) in chars {
                        let item_name = format!("{item_name}.{}", modifiers.as_str());
                        output.push(SymbolEntry::new(*value, &item_name));
                        // TODO report deprecations?
                    }
                }
            },
            Def::Module(module) => {
                collect_symbols(&module, &item_name, output);
            }
        }
    }
}

fn math_class_name(clazz: MathClass) -> &'static str {
    // NOTE: When adding values here, also update core/katvan_symbolpicker.cpp
    match clazz {
        MathClass::Normal => "Normal",
        MathClass::Alphabetic => "Alphabetic",
        MathClass::Binary => "Binary",
        MathClass::Closing => "Closing",
        MathClass::Diacritic => "Diacritic",
        MathClass::Fence => "Fence",
        MathClass::Large => "Large",
        MathClass::Opening => "Opening",
        MathClass::Punctuation => "Punctuation",
        MathClass::Relation => "Relation",
        MathClass::Space => "Space",
        MathClass::Unary => "Unary",
        MathClass::GlyphPart | MathClass::Vary | MathClass::Special => "Other",
    }
}
