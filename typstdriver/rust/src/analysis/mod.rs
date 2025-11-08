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
use typst::{
    Document,
    foundations::StyleChain,
    layout::{Frame, PagedDocument},
    model::{HeadingElem, Outlinable},
    syntax::{LinkedNode, Side, Source, Span, SyntaxKind},
};
use typst_ide::IdeWorld;

use crate::bridge::ffi;

pub(crate) mod tooltip;

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
