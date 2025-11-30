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
use std::path::Path;

use anyhow::{Context, Result};
use typst::{
    World,
    layout::{Abs, PagedDocument},
    visualize::Color,
};

use crate::{bridge::ffi, engine::DiagnosticsLogger};

pub fn export_pdf(
    document: &PagedDocument,
    world: &dyn World,
    logger: &ffi::LoggerProxy,
    path: &str,
    pdf_version: &str,
    pdfa_standard: &str,
    tagged: bool,
) -> Result<bool> {
    let options = pdf_options(pdf_version, pdfa_standard, tagged).context("Invalid options")?;

    let start = std::time::Instant::now();
    let result = typst_pdf::pdf(document, &options);

    let elapsed = format!("{:.2?}", start.elapsed());

    if let Ok(data) = result {
        let display_path = get_display_path(path);

        match std::fs::write(path, data) {
            Ok(()) => {
                logger.log_note(&format!(
                    "PDF exported successfully to {display_path} in {elapsed}"
                ));
                Ok(true)
            }
            Err(err) => {
                logger.log_error(
                    &format!("Unable to write to {display_path}: {err}"),
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
        logger.log_diagnostics(world, &result.unwrap_err());
        Ok(false)
    }
}

fn pdf_options(
    pdf_version: &str,
    pdfa_standard: &str,
    tagged: bool,
) -> Result<typst_pdf::PdfOptions<'static>> {
    let mut standards: Vec<typst_pdf::PdfStandard> = vec![];
    if !pdf_version.is_empty() {
        standards.push(serde_json::from_str(&format!("\"{pdf_version}\""))?);
    }
    if !pdfa_standard.is_empty() {
        standards.push(serde_json::from_str(&format!("\"{pdfa_standard}\""))?);
    }

    let standards = if standards.is_empty() {
        typst_pdf::PdfStandards::default()
    } else {
        typst_pdf::PdfStandards::new(&standards)
            .ok()
            .context("Incompatible version and PDF/A standard")?
    };

    Ok(typst_pdf::PdfOptions {
        standards,
        tagged,
        ..Default::default()
    })
}

pub fn export_png(
    document: &PagedDocument,
    logger: &ffi::LoggerProxy,
    path: &str,
    dpi: u32,
) -> bool {
    let pixel_per_pt = convert_dpi(dpi);

    let start = std::time::Instant::now();
    let pixmap =
        typst_render::render_merged(document, pixel_per_pt, Abs::zero(), Some(Color::WHITE));

    let elapsed = format!("{:.2?}", start.elapsed());
    let display_path = get_display_path(path);

    match pixmap.save_png(path) {
        Ok(()) => {
            logger.log_note(&format!(
                "PNG exported successfully to {display_path} in {elapsed}"
            ));
            true
        }
        Err(err) => {
            logger.log_error(
                &format!("Unable to write to {display_path}: {err}"),
                "",
                -1,
                -1,
                -1,
                -1,
                Vec::new(),
            );
            false
        }
    }
}

pub fn export_png_multi(
    document: &PagedDocument,
    logger: &ffi::LoggerProxy,
    dir: &str,
    name_pattern: &str,
    dpi: u32,
) -> bool {
    let pixel_per_pt = convert_dpi(dpi);
    let dir = Path::new(dir);

    let start = std::time::Instant::now();

    for page in &document.pages {
        let pixmap = typst_render::render(page, pixel_per_pt);

        let name = process_name_pattern(name_pattern, page.number, document.pages.len());
        let path = dir.join(name);

        if let Err(err) = pixmap.save_png(&path) {
            let display_path = get_display_path(&path);
            logger.log_error(
                &format!("Unable to write to {display_path}: {err}"),
                "",
                -1,
                -1,
                -1,
                -1,
                Vec::new(),
            );
            return false;
        }
    }

    let elapsed = format!("{:.2?}", start.elapsed());
    let display_path = get_display_path(dir);

    logger.log_note(&format!(
        "PNG set exported successfully to {display_path} in {elapsed}"
    ));
    true
}

fn process_name_pattern(pattern: &str, page: u64, total_pages: usize) -> String {
    let total_pages_width = (total_pages.ilog10() + 1) as usize;

    pattern
        .replace("{p}", &page.to_string())
        .replace("{n}", &format!("{page:0>total_pages_width$}"))
        .replace("{t}", &total_pages.to_string())
}

#[allow(clippy::cast_precision_loss)]
fn convert_dpi(dpi: u32) -> f32 {
    (dpi as f32) / 72.0
}

fn get_display_path(path: impl AsRef<Path>) -> String {
    crate::pathmap::get_display_path(path)
        .to_string_lossy()
        .into_owned()
}
