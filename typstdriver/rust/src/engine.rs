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
use typst::World;

use crate::bridge::ffi;
use crate::world::KatvanWorld;

pub struct EngineImpl<'a> {
    logger: &'a ffi::LoggerProxy,
    instance_id: String,
    world: KatvanWorld<'a>,
    result: Option<typst::model::Document>,
}

impl<'a> EngineImpl<'a> {
    pub fn new(
        logger: &'a ffi::LoggerProxy,
        package_manager: &'a ffi::PackageManagerProxy,
        instance_id: &str,
        root: &str,
    ) -> Self {
        EngineImpl {
            logger,
            instance_id: String::from(instance_id),
            world: KatvanWorld::new(package_manager, root),
            result: None,
        }
    }

    pub fn compile(&mut self, source: &str) -> Vec<u8> {
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

            let pdf = typst_pdf::pdf(
                &doc,
                typst::foundations::Smart::Custom(&self.instance_id),
                None,
            );

            self.result = Some(doc);
            pdf
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
            let id = match diag.span.id() {
                Some(id) => id,
                None => continue,
            };

            let source = self.world.source(id).unwrap();

            let package = id
                .package()
                .map(|pkg| pkg.to_string() + "/")
                .unwrap_or_default();
            let file = id.vpath().as_rootless_path().to_string_lossy();

            let range = source.range(diag.span).unwrap();
            let line = source.byte_to_line(range.start).unwrap_or(0);
            let col = source.byte_to_column(range.start).unwrap_or(0);

            let msg = format!(
                "{}{}:{}:{}: {}: {}",
                package,
                file,
                line + 1,
                col,
                match diag.severity {
                    typst::diag::Severity::Error => "error",
                    typst::diag::Severity::Warning => "warning",
                },
                diag.message
            );
            self.logger.log_to_batch(&msg);
        }
    }
}
