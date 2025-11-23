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
use std::pin::Pin;

use crate::engine::EngineImpl;

#[allow(clippy::needless_lifetimes)]
#[cxx::bridge(namespace = "katvan::typstdriver")]
pub(crate) mod ffi {
    enum PackageManagerError {
        Success,
        NotAllowed,
        NotFound,
        NetworkError,
        IoError,
        ArchiveError,
        ParseError,
    }

    struct PackageEntry {
        name: String,
        version: String,
        description: String,
    }

    struct PreviewPageDataInternal {
        page_num: u64,
        width_pts: f64,
        height_pts: f64,
        fingerprint: u64,
    }

    struct RenderedPage {
        width_px: u32,
        height_px: u32,
        buffer: Vec<u8>,
    }

    struct PreviewPosition {
        page: usize,
        x_pts: f64,
        y_pts: f64,
    }

    #[derive(Default, Hash)]
    struct SourcePosition {
        line: usize,
        column: usize,
    }

    struct ToolTip {
        content: String,
        details_url: String,
    }

    struct Completions {
        from: SourcePosition,
        completions_json: String,
    }

    struct DefinitionLocation {
        in_std: bool,
        position: SourcePosition,
    }

    #[derive(Hash)]
    struct OutlineEntry {
        level: usize,
        title: String,
        has_position: bool,
        position: SourcePosition,
    }

    #[derive(Hash)]
    struct LabelEntry {
        name: String,
        has_position: bool,
        position: SourcePosition,
    }

    struct DocumentMetadata {
        outline: Vec<OutlineEntry>,
        labels: Vec<LabelEntry>,
        fingerprint: u64,
    }

    unsafe extern "C++" {
        include!("typstdriver_logger_p.h");

        type LoggerProxy;

        #[rust_name = "log_note"]
        fn logNote(&self, message: &str);

        #[allow(clippy::too_many_arguments)]
        #[rust_name = "log_warning"]
        fn logWarning(
            &self,
            message: &str,
            file: &str,
            start_line: i64,
            start_col: i64,
            end_line: i64,
            end_col: i64,
            hints: Vec<&str>,
        );

        #[allow(clippy::too_many_arguments)]
        #[rust_name = "log_error"]
        fn logError(
            &self,
            message: &str,
            file: &str,
            start_line: i64,
            start_col: i64,
            end_line: i64,
            end_col: i64,
            hints: Vec<&str>,
        );
    }

    unsafe extern "C++" {
        include!("typstdriver_packagemanager_p.h");

        type PackageManagerProxy;

        #[rust_name = "get_package_local_path"]
        fn getPackageLocalPath(
            self: Pin<&mut PackageManagerProxy>,
            package_namespace: &str,
            name: &str,
            version: &str,
        ) -> String;

        #[rust_name = "get_preview_packages_listing"]
        fn getPreviewPackagesListing(self: Pin<&mut PackageManagerProxy>) -> Vec<PackageEntry>;

        fn error(&self) -> PackageManagerError;

        #[rust_name = "error_message"]
        fn errorMessage(&self) -> String;
    }

    extern "Rust" {
        type EngineImpl<'a>;

        fn set_source(&mut self, text: &str);

        fn apply_content_edit(&mut self, from_utf16_idx: usize, to_utf16_idx: usize, text: &str);

        fn compile(&mut self, now: &str) -> Vec<PreviewPageDataInternal>;

        fn render_page(&self, page: usize, point_size: f32) -> Result<RenderedPage>;

        fn export_pdf(
            &self,
            path: &str,
            pdf_version: &str,
            pdfa_standard: &str,
            tagged: bool,
        ) -> Result<bool>;

        fn forward_search(&self, line: usize, column: usize) -> Result<Vec<PreviewPosition>>;

        fn inverse_search(&self, pos: &PreviewPosition) -> Result<SourcePosition>;

        fn get_tooltip(&self, line: usize, column: usize) -> Result<ToolTip>;

        fn get_completions(
            &self,
            line: usize,
            column: usize,
            implicit: bool,
        ) -> Result<Completions>;

        fn get_definition(&self, line: usize, column: usize) -> Result<DefinitionLocation>;

        fn get_metadata(&self) -> Result<DocumentMetadata>;

        fn count_page_words(&self, page: usize) -> Result<usize>;

        fn set_allowed_paths(&mut self, paths: Vec<String>);

        fn discard_lookup_caches(&mut self);

        unsafe fn create_engine_impl<'a>(
            logger: &'a LoggerProxy,
            package_manager: Pin<&'a mut PackageManagerProxy>,
            root: &str,
        ) -> Box<EngineImpl<'a>>;

        fn typst_version() -> String;

        fn get_all_symbols_json() -> Result<String>;
    }
}

unsafe impl Send for ffi::PackageManagerProxy {}

fn create_engine_impl<'a>(
    logger: &'a ffi::LoggerProxy,
    package_manager: Pin<&'a mut ffi::PackageManagerProxy>,
    root: &str,
) -> Box<EngineImpl<'a>> {
    Box::new(EngineImpl::new(logger, package_manager, root))
}

fn typst_version() -> String {
    typst::syntax::package::PackageVersion::compiler().to_string()
}

fn get_all_symbols_json() -> anyhow::Result<String> {
    crate::symbols::symbols_json()
}
