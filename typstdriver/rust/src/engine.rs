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

use typst::World;

use crate::bridge::ffi;
use crate::world::KatvanWorld;

pub struct EngineImpl<'a> {
    logger: &'a ffi::LoggerProxy,
    world: KatvanWorld<'a>,
    result: Option<typst::model::Document>,
}

impl<'a> EngineImpl<'a> {
    pub fn new(
        logger: &'a ffi::LoggerProxy,
        package_manager: &'a ffi::PackageManagerProxy,
        root: &str,
    ) -> Self {
        EngineImpl {
            logger,
            world: KatvanWorld::new(package_manager, root),
            result: None,
        }
    }

    pub fn compile(&mut self, source: &str) -> Vec<ffi::PreviewPageDataInternal> {
        self.world.reset_source(source);

        self.logger.log_one("compiling ...");

        let start = std::time::Instant::now();
        let mut tracer = typst::eval::Tracer::new();
        let res = typst::compile(&self.world, &mut tracer);

        let elapsed = format!("{:.2?}", start.elapsed());
        let warnings = tracer.warnings();

        if let Ok(doc) = res {
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
            let errors = res.unwrap_err();

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

    fn try_render_page(&self, page: usize, point_size: f32) -> Option<ffi::RenderedPage> {
        let frame = &self.result.as_ref()?.pages.get(page)?.frame;
        let pixmap = typst_render::render(frame, point_size, typst::visualize::Color::WHITE);

        Some(ffi::RenderedPage {
            width_px: pixmap.width(),
            height_px: pixmap.height(),
            buffer: pixmap.take(),
        })
    }

    pub fn render_page(&self, page: usize, point_size: f32) -> ffi::RenderedPage {
        self.try_render_page(page, point_size).unwrap_or_default()
    }

    pub fn export_pdf(&self, path: &str) -> Result<(), Box<dyn std::error::Error>> {
        let document = self.result.as_ref().ok_or("Invalid state")?;
        let data = typst_pdf::pdf(document, typst::foundations::Smart::Auto, None);

        std::fs::write(path, data).map_err(|err| err.into())
    }

    fn try_forward_search(&self, line: usize, column: usize) -> Option<ffi::PreviewPosition> {
        let main = self.world.main();
        let cursor = main.line_column_to_byte(line, column)?;

        let pos = typst_ide::jump_from_cursor(self.result.as_ref()?, &main, cursor)?;

        Some(ffi::PreviewPosition {
            valid: true,
            page: usize::from(pos.page) - 1,
            x_pts: pos.point.x.to_pt(),
            y_pts: pos.point.y.to_pt(),
        })
    }

    pub fn foward_search(&self, line: usize, column: usize) -> ffi::PreviewPosition {
        self.try_forward_search(line, column).unwrap_or_default()
    }
}

fn hash_frame(frame: &typst::layout::Frame) -> u64 {
    let mut hasher = DefaultHasher::new();
    frame.hash(&mut hasher);
    hasher.finish()
}
