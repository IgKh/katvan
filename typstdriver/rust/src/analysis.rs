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
use pulldown_cmark::{Event, Tag};
use typst::{
    foundations::{Func, Value},
    layout::PagedDocument,
    syntax::{ast, LinkedNode, Side, Source},
};
use typst_ide::{analyze_expr, IdeWorld, Tooltip};

use crate::bridge::ffi;

const ONLINE_DOCS_PREFIX: &str = "https://typst.app/docs";

pub fn get_tooltip(
    world: &dyn IdeWorld,
    document: Option<&PagedDocument>,
    source: &Source,
    cursor: usize,
) -> Option<ffi::ToolTip> {
    get_documentation_tooltip(world, source, cursor).or_else(|| {
        let tooltip = typst_ide::tooltip(world, document, source, cursor, Side::Before)?;

        let content = match tooltip {
            Tooltip::Text(val) => val.to_string(),
            Tooltip::Code(val) => {
                let escaped = html_escape(&val);
                format!("<code><pre>{escaped}</pre></code>")
            }
        };

        Some(ffi::ToolTip {
            content,
            details_url: String::new(),
        })
    })
}

fn get_documentation_tooltip(
    world: &dyn IdeWorld,
    source: &Source,
    cursor: usize,
) -> Option<ffi::ToolTip> {
    let leaf = LinkedNode::new(source.root()).leaf_at(cursor, Side::Before)?;
    if leaf.kind().is_trivia() {
        return None;
    }

    let mut ancesstor = &leaf;
    while !ancesstor.is::<ast::Expr>() {
        ancesstor = ancesstor.parent()?;
    }

    let values = analyze_expr(world, ancesstor);
    if let [(Value::Func(f), _)] = values.as_slice() {
        let content = process_docs_markdown(f.docs()?);
        let details_url = get_reference_link(world, f).unwrap_or_default();

        Some(ffi::ToolTip {
            content,
            details_url,
        })
    } else {
        None
    }
}

fn html_escape(value: &str) -> String {
    let mut result = String::new();
    for ch in value.chars() {
        match ch {
            '<' => result.push_str("&lt;"),
            '>' => result.push_str("&gt;"),
            '&' => result.push_str("&amp;"),
            '"' => result.push_str("&quot;"),
            _ => result.push(ch),
        }
    }
    result
}

fn process_docs_markdown(docs: &str) -> String {
    let parser = pulldown_cmark::Parser::new(docs);

    // Convert internal links into external links into the Typst online
    // documentation, and only use the overview part of the docs (until the
    // first sub-heading)
    let events = parser
        .map(|e| {
            if let Event::Start(Tag::Link {
                link_type,
                dest_url,
                title,
                id,
            }) = e
            {
                let mut url: String = dest_url.into_string();
                if url.starts_with('$') {
                    url = format!("{}/{}", ONLINE_DOCS_PREFIX, &url[1..]);
                }

                Event::Start(Tag::Link {
                    link_type,
                    dest_url: url.into(),
                    title,
                    id,
                })
            } else {
                e
            }
        })
        .take_while(|e| {
            !matches!(
                e,
                Event::Start(Tag::Heading {
                    level: _,
                    id: _,
                    classes: _,
                    attrs: _,
                })
            )
        });

    let mut html = String::new();
    pulldown_cmark::html::push_html(&mut html, events);
    html
}

fn get_reference_link(world: &dyn IdeWorld, f: &Func) -> Option<String> {
    // Lookup function name in standard library
    // FIXME: This needs extra work for functions that live in a sub-module of
    // the stdlib, e.g. `pdf.embed` or `html.elem`. However there first needs to
    // be some fix to https://github.com/typst/typst/issues/6115
    let name = f.name()?;
    let binding = world.library().global.scope().get(name)?;
    let categoy = binding.category()?.name();

    Some(format!(
        "{}/reference/{}/{}",
        ONLINE_DOCS_PREFIX, categoy, name
    ))
}
