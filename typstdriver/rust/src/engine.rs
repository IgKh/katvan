/*
 * This file is part of Katvan
 * Copyright (c) 2024 Igor Khanin
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
use crate::world::{KatvanWorld, MAIN_ID};

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

    pub fn compile(&mut self, source: &str, now: &str) -> Vec<ffi::PreviewPageDataInternal> {
        self.world.reset_source(source, now);

        self.logger.log_one("compiling ...");

        let start = std::time::Instant::now();
        let res = typst::compile(&self.world);

        let elapsed = format!("{:.2?}", start.elapsed());
        let warnings = res.warnings;

        if let Ok(doc) = res.output {
            if warnings.is_empty() {
                self.logger
                    .log_to_batch(&format!("compiled successfully in {elapsed}"));
            } else {
                self.logger
                    .log_to_batch(&format!("compiled with warnings in {elapsed}"));
            }

            self.log_diagnostics(&warnings);
            self.logger.release_batched();

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

            self.logger
                .log_to_batch(&format!("compiled with errors in {elapsed}"));
            self.log_diagnostics(&errors);
            self.log_diagnostics(&warnings);
            self.logger.release_batched();

            Vec::new()
        }
    }

    fn log_diagnostics(&self, diagnostics: &[typst::diag::SourceDiagnostic]) {
        for diag in diagnostics {
            let msg = format!(
                "{}: {}: {}",
                self.span_to_location(diag.span),
                match diag.severity {
                    typst::diag::Severity::Error => "error",
                    typst::diag::Severity::Warning => "warning",
                },
                diag.message
            );
            self.logger.log_to_batch(&msg);

            for trace in &diag.trace {
                let msg = format!("{}: note: {}", self.span_to_location(trace.span), trace.v);
                self.logger.log_to_batch(&msg);
            }

            for hint in &diag.hints {
                self.logger.log_to_batch(&format!("hint: {hint}"));
            }
        }
    }

    fn span_to_location(&self, span: typst::syntax::Span) -> String {
        let id = match span.id() {
            Some(id) => id,
            None => return String::new(),
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

        format!("{}{}:{}:{}", package, file, line + 1, col)
    }

    pub fn render_page(&self, page: usize, point_size: f32) -> Result<ffi::RenderedPage> {
        let document = self.result.as_ref().context("Invalid state")?;
        let page = document.pages.get(page).context("No such page")?;
        let pixmap = typst_render::render(page, point_size);

        Ok(ffi::RenderedPage {
            width_px: pixmap.width(),
            height_px: pixmap.height(),
            buffer: pixmap.take(),
        })
    }

    pub fn export_pdf(&self, path: &str) -> Result<()> {
        let document = self.result.as_ref().context("Invalid state")?;

        let options = typst_pdf::PdfOptions {
            ident: typst::foundations::Smart::Auto,
            timestamp: None,
            page_ranges: None,
            standards: typst_pdf::PdfStandards::default(),
        };

        let result = typst_pdf::pdf(document, &options);
        if let Ok(data) = result {
            std::fs::write(path, data).map_err(|err| err.into())
        } else {
            // TODO: PDF generation now can return diagnostics just like
            // compilation. Find a good way to display those.
            Err(anyhow::anyhow!("PDF generation failed"))
        }
    }

    pub fn foward_search(&self, line: usize, column: usize) -> Result<ffi::PreviewPosition> {
        let document = self.result.as_ref().context("Invalid state")?;
        let main = self.world.main_source();

        let cursor = main
            .line_column_to_byte(line, column)
            .context("No such position")?;

        // TODO: There can be multiple positions returned since the same syntax
        // node can appear in multiple places in the rendered document (e.g. a
        // header can also appear in a TOC). According to the Typst Discord,
        // the intention is to pick the match that is closest to the previewer's
        // current scroll position. We should implement that too eventually, but
        // for now just pick the first one.
        let positions = typst_ide::jump_from_cursor(document, &main, cursor);

        match positions.first() {
            Some(pos) => Ok(ffi::PreviewPosition {
                page: usize::from(pos.page) - 1,
                x_pts: pos.point.x.to_pt(),
                y_pts: pos.point.y.to_pt(),
            }),
            None => Err(anyhow::anyhow!("Position not found in preview")),
        }
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
}

fn hash_frame(frame: &typst::layout::Frame) -> u64 {
    let mut hasher = DefaultHasher::new();
    frame.hash(&mut hasher);
    hasher.finish()
}
