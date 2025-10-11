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
use pulldown_cmark::{BrokenLink, CowStr, Event, Tag};
use typst::{
    foundations::{Func, StyleChain, Value},
    layout::{Frame, PagedDocument},
    model::{HeadingElem, Outlinable},
    syntax::{ast, LinkedNode, Side, Source, Span, SyntaxKind},
    Document,
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
        let tooltip = typst_ide::tooltip(world, document, source, cursor, Side::After)?;

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
        let content = process_docs_markdown(world, f.docs()?);
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

fn process_docs_markdown(world: &dyn IdeWorld, docs: &str) -> String {
    let parser = pulldown_cmark::Parser::new_with_broken_link_callback(
        docs,
        pulldown_cmark::Options::empty(),
        Some(|link: BrokenLink| process_docs_broken_link(world, link)),
    );

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

fn process_docs_broken_link<'a>(
    world: &dyn IdeWorld,
    link: BrokenLink,
) -> Option<(CowStr<'a>, CowStr<'a>)> {
    if link.link_type != pulldown_cmark::LinkType::Shortcut {
        return None;
    }

    let reference = link.reference.into_string();
    let name = reference.trim_matches('`');

    get_reference_link_by_name(world, name).map(move |url| (url.into(), reference.into()))
}

fn get_reference_link(world: &dyn IdeWorld, f: &Func) -> Option<String> {
    // Lookup function name in standard library
    // FIXME: This needs extra work for functions that live in a sub-module of
    // the stdlib, e.g. `pdf.embed` or `html.elem`. However there first needs to
    // be some fix to https://github.com/typst/typst/issues/6115
    get_reference_link_by_name(world, f.name()?)
}

fn get_reference_link_by_name(world: &dyn IdeWorld, name: &str) -> Option<String> {
    let binding = world.library().global.scope().get(name)?;
    let category = binding.category()?.name();

    Some(format!("{ONLINE_DOCS_PREFIX}/reference/{category}/{name}"))
}

pub fn get_definition(
    world: &dyn IdeWorld,
    document: Option<&PagedDocument>,
    source: &Source,
    cursor: usize,
) -> Option<ffi::DefinitionLocation> {
    let definition = typst_ide::definition(world, document, source, cursor, Side::After)?;

    match definition {
        typst_ide::Definition::Std(_) => Some(ffi::DefinitionLocation {
            in_std: true,
            position: ffi::SourcePosition::default(),
        }),
        typst_ide::Definition::Span(span) => {
            let id = span.id().unwrap();
            if id == source.id() {
                Some(ffi::DefinitionLocation {
                    in_std: false,
                    position: span_location(source, span)?,
                })
            } else {
                // TODO try to at least find the import in the current file
                None
            }
        }
    }
}

pub fn get_metadata(
    document: &PagedDocument,
    source: &Source,
) -> (Vec<ffi::OutlineEntry>, Vec<ffi::LabelEntry>) {
    let introspector = document.introspector();
    let styles = StyleChain::default();

    let mut outline: Vec<ffi::OutlineEntry> = Vec::new();
    let mut labels: Vec<ffi::LabelEntry> = Vec::new();

    for item in introspector.all() {
        if let Some(label) = item.label() {
            let position = span_location(source, item.span());
            labels.push(ffi::LabelEntry {
                name: label.resolve().as_str().to_owned(),
                has_position: position.is_some(),
                position: position.unwrap_or_default(),
            });
        }

        if let Some(heading) = item.to_packed::<HeadingElem>() {
            if !heading.outlined() {
                continue;
            }

            let position = find_text_position_for_span(source, heading.span());
            outline.push(ffi::OutlineEntry {
                level: heading.resolve_level(styles).into(),
                title: heading.body.plain_text().to_string(),
                has_position: position.is_some(),
                position: position.unwrap_or_default(),
            });
        }
    }

    (outline, labels)
}

fn find_text_position_for_span(source: &Source, span: Span) -> Option<ffi::SourcePosition> {
    // When we want a source position that will be jumped to for a span, prefer
    // to take the start of the first text node if any, since jumping from cursor
    // only works on text.
    let text_span = LinkedNode::new(source.root())
        .find(span)
        .and_then(find_first_text_node)
        .map(|node| node.span());

    let span = text_span.unwrap_or(span);
    span_location(source, span)
}

fn span_location(source: &Source, span: Span) -> Option<ffi::SourcePosition> {
    source.range(span).and_then(|span| {
        Some(ffi::SourcePosition {
            line: source.lines().byte_to_line(span.start)?,
            column: source.lines().byte_to_column(span.start)?,
        })
    })
}

fn find_first_text_node(node: LinkedNode<'_>) -> Option<LinkedNode<'_>> {
    if node.kind() == SyntaxKind::Text {
        return Some(node.clone());
    }

    for child in node.children() {
        if let Some(node) = find_first_text_node(child) {
            return Some(node);
        }
    }
    None
}

pub fn count_words(frame: &Frame) -> usize {
    use unicode_segmentation::UnicodeSegmentation;

    let mut count: usize = 0;

    for (_, item) in frame.items() {
        match item {
            typst::layout::FrameItem::Group(group) => {
                count += count_words(&group.frame);
            }
            typst::layout::FrameItem::Text(text) => {
                // FIXME: Wrong counts due to https://github.com/typst/typst/issues/6596
                count += text.text.unicode_words().count();
            }
            _ => {}
        }
    }
    count
}
