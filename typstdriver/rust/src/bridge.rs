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
use std::pin::Pin;

use crate::engine::EngineImpl;

#[allow(clippy::needless_lifetimes)]
#[cxx::bridge(namespace = "katvan::typstdriver")]
pub(crate) mod ffi {
    enum PackageManagerError {
        Success,
        NotFound,
        NetworkError,
        IoError,
        ArchiveError,
    }

    struct PreviewPageDataInternal {
        page_num: usize,
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

    struct SourcePosition {
        line: usize,
        column: usize,
    }

    unsafe extern "C++" {
        include!("typstdriver_logger_p.h");

        type LoggerProxy;

        #[rust_name = "log_note"]
        fn logNote(&self, message: &str);

        #[rust_name = "log_warning"]
        fn logWarning(&self, message: &str, file: &str, line: i64, col: i64, hints: Vec<&str>);

        #[rust_name = "log_error"]
        fn logError(&self, message: &str, file: &str, line: i64, col: i64, hints: Vec<&str>);
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

        fn error(&self) -> PackageManagerError;

        #[rust_name = "error_message"]
        fn errorMessage(&self) -> String;
    }

    extern "Rust" {
        type EngineImpl<'a>;

        fn compile(&mut self, source: &str, now: &str) -> Vec<PreviewPageDataInternal>;

        fn render_page(&self, page: usize, point_size: f32) -> Result<RenderedPage>;

        fn export_pdf(&self, path: &str) -> Result<bool>;

        fn foward_search(&self, line: usize, column: usize) -> Result<Vec<PreviewPosition>>;

        fn inverse_search(&self, pos: &PreviewPosition) -> Result<SourcePosition>;

        unsafe fn create_engine_impl<'a>(
            logger: &'a LoggerProxy,
            package_manager: Pin<&'a mut PackageManagerProxy>,
            root: &str,
        ) -> Box<EngineImpl<'a>>;

        fn typst_version() -> String;
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
