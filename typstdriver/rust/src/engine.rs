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
use std::hash::{DefaultHasher, Hash, Hasher};
use std::pin::Pin;

use anyhow::{Context, Result};
use typst::layout::{Abs, Point};
use typst::World;

use crate::bridge::ffi;
use crate::darkifier;
use crate::world::{KatvanWorld, MAIN_ID};

#[derive(Debug)]
struct DiagnosticLocation {
    file: String,
    line: i64,
    column: i64,
}

impl Default for DiagnosticLocation {
    fn default() -> Self {
        Self {
            file: String::new(),
            line: -1,
            column: 0,
        }
    }
}

pub struct EngineImpl<'a> {
    logger: &'a ffi::LoggerProxy,
    world: KatvanWorld<'a>,
    result: Option<typst::model::Document>,
}

impl<'a> EngineImpl<'a> {
    pub fn new(
        logger: &'a ffi::LoggerProxy,
        package_manager: Pin<&'a mut ffi::PackageManagerProxy>,
        root: &str,
    ) -> Self {
        EngineImpl {
            logger,
            world: KatvanWorld::new(package_manager, root),
            result: None,
        }
    }

    pub fn set_source(&mut self, text: &str) {
        self.world.set_source_text(text);
    }

    pub fn apply_content_edit(&mut self, from_utf16_idx: usize, to_utf16_idx: usize, text: &str) {
        self.world.apply_edit(from_utf16_idx, to_utf16_idx, text);
    }

    pub fn compile(&mut self, now: &str) -> Vec<ffi::PreviewPageDataInternal> {
        self.world.reset_current_date(now);

        let start = std::time::Instant::now();
        let res = typst::compile(&self.world);

        let elapsed = format!("{:.2?}", start.elapsed());
        let warnings = res.warnings;

        if let Ok(doc) = res.output {
            self.log_diagnostics(&warnings);

            if warnings.is_empty() {
                self.logger
                    .log_note(&format!("compiled successfully in {elapsed}"));
            } else {
                self.logger
                    .log_note(&format!("compiled with warnings in {elapsed}"));
            }

            comemo::evict(5);

            let pages = doc
                .pages
                .iter()
                .map(|page| ffi::PreviewPageDataInternal {
                    page_num: page.number,
                    width_pts: page.frame.width().abs().to_pt(),
                    height_pts: page.frame.height().abs().to_pt(),
                    fingerprint: hash_frame(&page.frame),
                })
                .collect();

            self.result = Some(doc);
            pages
        } else {
            let errors = res.output.unwrap_err();

            self.log_diagnostics(&errors);
            self.log_diagnostics(&warnings);
            self.logger
                .log_note(&format!("compiled with errors in {elapsed}"));

            Vec::new()
        }
    }

    fn log_diagnostics(&self, diagnostics: &[typst::diag::SourceDiagnostic]) {
        for diag in diagnostics {
            // Find first span (of the diagnostic itself, or the traceback to
            // it) that is located in the main source.
            let span = std::iter::once(diag.span)
                .chain(diag.trace.iter().map(|t| t.span))
                .find(|span| span.id() == Some(*MAIN_ID))
                .unwrap_or(diag.span);

            let location = self.span_to_location(span);
            let hints = diag.hints.iter().map(|hint| hint.as_str()).collect();

            match diag.severity {
                typst::diag::Severity::Error => self.logger.log_error(
                    &diag.message,
                    &location.file,
                    location.line,
                    location.column,
                    hints,
                ),
                typst::diag::Severity::Warning => self.logger.log_warning(
                    &diag.message,
                    &location.file,
                    location.line,
                    location.column,
                    hints,
                ),
            };
        }
    }

    fn span_to_location(&self, span: typst::syntax::Span) -> DiagnosticLocation {
        let id = match span.id() {
            Some(id) => id,
            None => return DiagnosticLocation::default(),
        };

        let source = self.world.source(id).unwrap();

        let package = id
            .package()
            .map(|pkg| pkg.to_string() + "/")
            .unwrap_or_default();
        let file = id.vpath().as_rootless_path().to_string_lossy();

        let range = source.range(span).unwrap();
        let line = source.byte_to_line(range.start).unwrap_or(0);
        let col = source.byte_to_column(range.start).unwrap_or(0);

        DiagnosticLocation {
            file: format!("{}{}", package, file),
            line: (line + 1) as i64,
            column: col as i64,
        }
    }

    pub fn render_page(
        &self,
        page: usize,
        point_size: f32,
        invert_colors: bool,
    ) -> Result<ffi::RenderedPage> {
        let document = self.result.as_ref().context("Invalid state")?;
        let page = document.pages.get(page).context("No such page")?;

        let pixmap = if invert_colors {
            let page = darkifier::invert_page_colors(page);
            typst_render::render(&page, point_size)
        } else {
            typst_render::render(page, point_size)
        };

        Ok(ffi::RenderedPage {
            width_px: pixmap.width(),
            height_px: pixmap.height(),
            buffer: pixmap.take(),
        })
    }

    pub fn export_pdf(&self, path: &str) -> Result<bool> {
        let document = self.result.as_ref().context("Invalid state")?;

        let options = typst_pdf::PdfOptions {
            ident: typst::foundations::Smart::Auto,
            timestamp: None,
            page_ranges: None,
            standards: typst_pdf::PdfStandards::default(),
        };

        let start = std::time::Instant::now();
        let result = typst_pdf::pdf(document, &options);

        let elapsed = format!("{:.2?}", start.elapsed());

        if let Ok(data) = result {
            match std::fs::write(path, data) {
                Ok(_) => {
                    self.logger
                        .log_note(&format!("PDF exported successfully to {path} in {elapsed}"));
                    Ok(true)
                }
                Err(err) => {
                    self.logger.log_error(
                        &format!("Unable to write to {path}: {err}"),
                        "",
                        -1,
                        -1,
                        Vec::new(),
                    );
                    Ok(false)
                }
            }
        } else {
            self.log_diagnostics(&result.unwrap_err());
            Ok(false)
        }
    }

    pub fn foward_search(&self, line: usize, column: usize) -> Result<Vec<ffi::PreviewPosition>> {
        let document = self.result.as_ref().context("Invalid state")?;
        let main = self.world.main_source();

        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        let positions = typst_ide::jump_from_cursor(document, &main, cursor);

        let result = positions
            .into_iter()
            .map(|pos| ffi::PreviewPosition {
                page: usize::from(pos.page) - 1,
                x_pts: pos.point.x.to_pt(),
                y_pts: pos.point.y.to_pt(),
            })
            .collect();
        Ok(result)
    }

    fn convert_jump_to_source_position(
        &self,
        jump: typst_ide::Jump,
    ) -> Option<ffi::SourcePosition> {
        match jump {
            typst_ide::Jump::Source(id, cursor) if id == *MAIN_ID => {
                let source = self.world.main_source();

                Some(ffi::SourcePosition {
                    line: source.byte_to_line(cursor)?,
                    column: source.byte_to_column(cursor)?,
                })
            }
            _ => None,
        }
    }

    pub fn inverse_search(&self, pos: &ffi::PreviewPosition) -> Result<ffi::SourcePosition> {
        let document = self.result.as_ref().context("Invalid state")?;
        let page = document.pages.get(pos.page).context("Missing page")?;
        let click = Point::new(Abs::pt(pos.x_pts), Abs::pt(pos.y_pts));

        let jump = typst_ide::jump_from_click(&self.world, document, &page.frame, click)
            .context("Inverse search failed")?;

        self.convert_jump_to_source_position(jump)
            .context("Jump target not applicable")
    }

    pub fn get_tooltip(&self, line: usize, column: usize) -> Result<String> {
        let main = self.world.main_source();
        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        let tooltip = typst_ide::tooltip(
            &self.world,
            self.result.as_ref(),
            &main,
            cursor,
            typst::syntax::Side::Before,
        )
        .context("No available tooltip")?;

        match tooltip {
            typst_ide::Tooltip::Text(val) => Ok(val.to_string()),
            typst_ide::Tooltip::Code(val) => {
                let escaped = html_escape(&val);
                Ok(format!("<pre>{escaped}</pre>"))
            }
        }
    }

    pub fn get_completions(&self, line: usize, column: usize) -> Result<ffi::Completions> {
        let main = self.world.main_source();
        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        let (start_crusor, completions) =
            typst_ide::autocomplete(&self.world, self.result.as_ref(), &main, cursor, true)
                .context("No available completions")?;

        Ok(ffi::Completions {
            from: ffi::SourcePosition {
                line: main.byte_to_line(start_crusor).unwrap(),
                column: main.byte_to_column(start_crusor).unwrap(),
            },
            completions_json: serde_json::to_string(&completions)?,
        })
    }

    pub fn discard_lookup_caches(&mut self) {
        self.world.discard_package_roots_cache();
    }
}

fn hash_frame(frame: &typst::layout::Frame) -> u64 {
    let mut hasher = DefaultHasher::new();
    frame.hash(&mut hasher);
    hasher.finish()
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
