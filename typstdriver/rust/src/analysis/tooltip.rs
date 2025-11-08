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
    foundations::{Func, Scope, Type, Value},
    layout::PagedDocument,
    syntax::{
        LinkedNode, Side, Source, SyntaxKind,
        ast::{self, AstNode},
    },
};
use typst_ide::{IdeWorld, Tooltip, analyze_expr};

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

    let (name, docs) = match values.as_slice() {
        [(Value::Func(f), _)] => (f.name()?, f.docs()?),
        [(Value::Type(t), _)] => (t.short_name(), t.docs()),
        _ => return None,
    };

    let mut path = vec![];
    if let Some(parent) = ancesstor.parent()
        && parent.kind() == SyntaxKind::FieldAccess
        && ancesstor.index() > 0
    {
        path = resolve_field_target_path(world, source, parent.cast().unwrap()).unwrap_or_default();
    }
    path.push(name.to_string());

    let content = process_docs_markdown(world, docs);

    let path_refs = path.iter().map(String::as_ref).collect();
    let details_url = get_reference_link(world, path_refs).unwrap_or_default();

    Some(ffi::ToolTip {
        content,
        details_url,
    })
}

/// Resolves the target of a (possibly nested) field access into a fully
/// qualified search path in the standard library of its' type
fn resolve_field_target_path(
    world: &dyn IdeWorld,
    source: &Source,
    field: ast::FieldAccess<'_>,
) -> Option<Vec<String>> {
    let node = LinkedNode::new(source.root()).find(field.target().span())?;
    let values = analyze_expr(world, &node);

    let node_name = match values.as_slice() {
        [(Value::Module(m), _)] => m.name()?,
        [(Value::Func(f), _)] => f.name()?,
        [(Value::Type(t), _)] => t.short_name(),
        [(value, _)] => return Some(vec![value.ty().short_name().to_string()]),
        _ => return None,
    };

    if let ast::Expr::FieldAccess(nested) = field.target() {
        let mut prefixes = resolve_field_target_path(world, source, nested)?;
        prefixes.push(node_name.to_string());
        return Some(prefixes);
    }

    Some(vec![node_name.to_string()])
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
                    url = process_docs_internal_link(world, &url[1..]);
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

fn process_docs_internal_link(world: &dyn IdeWorld, path: &str) -> String {
    let (path, annex) = match path.find('/') {
        Some(pos) => path.split_at(pos),
        None => (path, ""),
    };

    let path_parts: Vec<_> = path.split('.').collect();
    let url =
        get_reference_link(world, path_parts).unwrap_or_else(|| get_well_known_page_link(path));

    if !annex.is_empty() && !url.contains('#') {
        url + annex
    } else {
        url
    }
}

fn get_well_known_page_link(path: &str) -> String {
    let page = match path {
        "tutorial" | "guides" | "reference" => path.to_string(),
        _ => format!("reference/{path}"),
    };

    format!("{ONLINE_DOCS_PREFIX}/{page}")
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
    let path: Vec<_> = name.split('.').collect();

    get_reference_link(world, path).map(move |url| (url.into(), reference.into()))
}

fn get_reference_link(world: &dyn IdeWorld, mut path: Vec<&str>) -> Option<String> {
    // TODO look in math scope if in math mode
    let scope = if path.first() == Some(&"std") {
        path.remove(0);
        world.library().std.read().scope()?
    } else {
        world.library().global.scope()
    };

    // Functions in standard library prelude
    // https://github.com/typst/typst/blob/42cf5ed23a29b956ce8187235b8493476bcee826/crates/typst-library/src/lib.rs#L313
    match path.first() {
        Some(&"luma" | &"oklab" | &"oklch" | &"rgb" | &"cmyk") => path.insert(0, "color"),
        Some(&"range") => path.insert(0, "array"),
        _ => {}
    }

    get_reference_link_in_scope(scope, &path)
}

fn get_reference_link_in_scope(scope: &Scope, path: &[&str]) -> Option<String> {
    let mut scope = scope;
    let empty_scope = Scope::new();

    let mut url: Option<String> = None;
    let mut in_func: Option<&Func> = None;

    for elem in path {
        if let Some(f) = in_func {
            // Look for arguments
            if f.param(elem).is_some() {
                let url = url.unwrap();
                if url.contains('#') {
                    return Some(format!("{url}-{elem}"));
                }
                return Some(format!("{url}#parameters-{elem}"));
            }
        }

        let binding = {
            let mut binding = scope.get(elem);
            if binding.is_none() {
                // If previous element was a function, also look at the scope of
                // the Func type (for with, where)
                if in_func.is_some() {
                    binding = Type::of::<Func>().scope().get(elem);
                }
                if binding.is_some() {
                    // If so - reset the url
                    url = Some(format!(
                        "{ONLINE_DOCS_PREFIX}/reference/foundations/function"
                    ));
                }
            }
            binding?
        };

        match binding.read() {
            Value::Module(m) => {
                // Modules don't have full documentation attached, just move
                // over to the next path element
                in_func = None;
                scope = m.scope();
            }
            Value::Type(t) => {
                in_func = None;
                scope = t.scope();

                let category = binding.category()?.name();
                url = Some(format!("{ONLINE_DOCS_PREFIX}/reference/{category}/{elem}"));
            }
            Value::Func(f) => {
                in_func = Some(f);
                scope = f.scope().unwrap_or(&empty_scope);

                if let Some(prev_url) = url {
                    // Page already set by a type, this function is a method
                    if prev_url.contains('#') {
                        url = Some(format!("{prev_url}-definitions-{elem}"));
                    } else {
                        url = Some(format!("{prev_url}#definitions-{elem}"));
                    }
                } else {
                    let category = binding.category()?.name();
                    url = Some(format!("{ONLINE_DOCS_PREFIX}/reference/{category}/{elem}"));
                }
            }
            _ => return None,
        }
    }
    url
}

#[cfg(test)]
mod tests {
    use crate::analysis::tooltip::ONLINE_DOCS_PREFIX;

    fn get_reference_link(world: &dyn typst_ide::IdeWorld, path: &str) -> Option<String> {
        let path: Vec<_> = path.split('.').collect();

        crate::analysis::tooltip::get_reference_link(world, path)
            .map(|s| s.trim_start_matches(ONLINE_DOCS_PREFIX).to_string())
    }

    #[test]
    fn test_get_reference_link_basic() {
        let world = crate::tests::TestWorld::new("");

        assert_eq!(
            get_reference_link(&world, "page"),
            Some(format!("/reference/layout/page"))
        );
        assert_eq!(
            get_reference_link(&world, "pdf.attach"),
            Some(format!("/reference/pdf/attach"))
        );
        assert_eq!(
            get_reference_link(&world, "std.pdf.attach"),
            Some(format!("/reference/pdf/attach"))
        );
        assert_eq!(
            get_reference_link(&world, "color"),
            Some(format!("/reference/visualize/color"))
        );
        assert_eq!(
            get_reference_link(&world, "color.rgb"),
            Some(format!("/reference/visualize/color#definitions-rgb"))
        );
        assert_eq!(
            get_reference_link(&world, "rgb"),
            Some(format!("/reference/visualize/color#definitions-rgb"))
        );
        assert_eq!(
            get_reference_link(&world, "outline.depth"),
            Some(format!("/reference/model/outline#parameters-depth"))
        );
        assert_eq!(
            get_reference_link(&world, "outline.entry.indented.prefix"),
            Some(format!(
                "/reference/model/outline#definitions-entry-definitions-indented-prefix"
            ))
        );
        assert_eq!(
            get_reference_link(&world, "outline.entry.where"),
            Some(format!("/reference/foundations/function#definitions-where"))
        );

        assert_eq!(get_reference_link(&world, "nosuchthing"), None);
        assert_eq!(get_reference_link(&world, "pdf.nosuchthing"), None);
    }
}
