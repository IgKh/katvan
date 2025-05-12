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
use typst::layout::{Abs, PagedDocument, Point};
use typst::World;

use crate::analysis;
use crate::bridge::ffi;
use crate::darkifier;
use crate::world::{KatvanWorld, MAIN_ID};

#[derive(Debug)]
struct DiagnosticFileLocation {
    line: i64,
    column: i64,
}

impl Default for DiagnosticFileLocation {
    fn default() -> Self {
        Self {
            line: -1,
            column: 0,
        }
    }
}

#[derive(Debug, Default)]
struct DiagnosticLocation {
    file: String,
    start: DiagnosticFileLocation,
    end: DiagnosticFileLocation,
}

pub struct EngineImpl<'a> {
    logger: &'a ffi::LoggerProxy,
    world: KatvanWorld<'a>,
    result: Option<PagedDocument>,
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
        let res = typst::compile::<PagedDocument>(&self.world);

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

            typst::comemo::evict(3);

            let pages = doc
                .pages
                .iter()
                .map(|page| ffi::PreviewPageDataInternal {
                    page_num: page.number,
                    width_pts: page.frame.width().abs().to_pt(),
                    height_pts: page.frame.height().abs().to_pt(),
                    fingerprint: calc_fingerprint(&page.frame),
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
            let hints = Self::get_diagnostic_hints(diag);

            match diag.severity {
                typst::diag::Severity::Error => self.logger.log_error(
                    &diag.message,
                    &location.file,
                    location.start.line,
                    location.start.column,
                    location.end.line,
                    location.end.column,
                    hints,
                ),
                typst::diag::Severity::Warning => self.logger.log_warning(
                    &diag.message,
                    &location.file,
                    location.start.line,
                    location.start.column,
                    location.end.line,
                    location.end.column,
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
        let start = position_to_file_location(&source, range.start).unwrap_or_default();
        let end = position_to_file_location(&source, range.end).unwrap_or_default();

        DiagnosticLocation {
            file: format!("{}{}", package, file),
            start,
            end,
        }
    }

    fn get_diagnostic_hints(diag: &typst::diag::SourceDiagnostic) -> Vec<&str> {
        // As per https://github.com/typst/typst/blob/0.12/crates/typst/src/diag.rs#L311
        if diag.message.contains("(access denied)") {
            vec![
                "by default cannot read file outside of document's directory",
                "additional allowed paths can be set in compiler settings",
            ]
        } else {
            diag.hints.iter().map(|hint| hint.as_str()).collect()
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

    pub fn forward_search(&self, line: usize, column: usize) -> Result<Vec<ffi::PreviewPosition>> {
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
            typst_ide::Jump::File(id, cursor) if id == *MAIN_ID => {
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

    pub fn get_tooltip(&self, line: usize, column: usize) -> Result<ffi::ToolTip> {
        let main = self.world.main_source();
        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        analysis::get_tooltip(&self.world, self.result.as_ref(), &main, cursor)
            .context("No available tooltip")
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

    pub fn get_definition(&self, line: usize, column: usize) -> Result<ffi::DefinitionLocation> {
        let main = self.world.main_source();
        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        analysis::get_definition(&self.world, self.result.as_ref(), &main, cursor)
            .context("Definition not found")
    }

    pub fn get_outline(&self) -> Result<ffi::Outline> {
        let doc = self.result.as_ref().context("Not compiled yet")?;
        let main = self.world.main_source();

        let entries = analysis::get_outline(doc, &main);
        let fingerprint = calc_fingerprint(&entries);

        Ok(ffi::Outline {
            entries,
            fingerprint,
        })
    }

    pub fn set_allowed_paths(&mut self, paths: Vec<String>) {
        self.world.set_allowed_paths(paths);
    }

    pub fn discard_lookup_caches(&mut self) {
        self.world.discard_package_roots_cache();
    }
}

fn position_to_file_location(
    source: &typst::syntax::Source,
    byte_idx: usize,
) -> Option<DiagnosticFileLocation> {
    let line = source.byte_to_line(byte_idx)?;
    let column = source.byte_to_column(byte_idx)?;

    Some(DiagnosticFileLocation {
        line: line as i64,
        column: column as i64,
    })
}

fn calc_fingerprint<H: Hash>(item: &H) -> u64 {
    let mut hasher = DefaultHasher::new();
    item.hash(&mut hasher);
    hasher.finish()
}
