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
use std::{
    collections::HashMap,
    path::PathBuf,
    pin::Pin,
    sync::{LazyLock, Mutex},
};

use time::{OffsetDateTime, format_description::well_known::Iso8601};
use typst::{
    Feature, Library, LibraryExt,
    diag::{EcoString, FileError, FileResult, PackageError},
    foundations::Bytes,
    syntax::{FileId, Source, VirtualPath, package::PackageSpec},
    text::{Font, FontBook},
    utils::LazyHash,
};
use typst_kit::fonts::{FontSlot, Fonts};

use crate::bridge::ffi;
use crate::pathmap;

pub static MAIN_ID: LazyLock<FileId> = LazyLock::new(|| FileId::new_fake(VirtualPath::new("MAIN")));

pub struct KatvanWorld<'a> {
    path_mapper: pathmap::PathMapper,
    package_manager: Mutex<PackageManagerWrapper<'a>>,
    packages_list: once_cell::sync::OnceCell<Vec<(PackageSpec, Option<EcoString>)>>,
    library: LazyHash<Library>,
    book: LazyHash<FontBook>,
    fonts: Vec<FontSlot>,
    source: Source,
    now: Option<OffsetDateTime>,
}

impl<'a> KatvanWorld<'a> {
    pub fn new(package_manager: Pin<&'a mut ffi::PackageManagerProxy>, root: &str) -> Self {
        let font_dirs: Vec<PathBuf> = std::env::var_os("TYPST_FONT_PATHS")
            .map(|p| std::env::split_paths(&p).collect())
            .unwrap_or_default();

        let fonts = Fonts::searcher().search_with(font_dirs);
        let source = Source::new(*MAIN_ID, String::new());

        KatvanWorld {
            path_mapper: pathmap::PathMapper::new(root),
            package_manager: Mutex::new(PackageManagerWrapper::new(package_manager)),
            packages_list: once_cell::sync::OnceCell::new(),
            library: LazyHash::new(Library::default()),
            book: LazyHash::new(fonts.book),
            fonts: fonts.fonts,
            source,
            now: None,
        }
    }

    pub fn set_source_text(&mut self, text: &str) {
        self.source.replace(text);
    }

    pub fn apply_edit(&mut self, from_utf16_idx: usize, to_utf16_idx: usize, text: &str) {
        let from = self.source.lines().utf16_to_byte(from_utf16_idx);
        let to = self.source.lines().utf16_to_byte(to_utf16_idx);

        if let (Some(from), Some(to)) = (from, to) {
            self.source.edit(from..to, text);
        }
    }

    pub fn reset_current_date(&mut self, now: &str) {
        self.now = OffsetDateTime::parse(now, &Iso8601::PARSING).ok();
    }

    pub fn discard_package_roots_cache(&mut self) {
        let mut manager = self.package_manager.lock().unwrap();
        manager.roots_cache.clear();
    }

    pub fn set_compiler_flags(&mut self, a11y_extras: bool) {
        let mut features = vec![];
        if a11y_extras {
            features.push(Feature::A11yExtras);
        }

        self.library = LazyHash::new(
            Library::builder()
                .with_features(features.into_iter().collect())
                .build(),
        );
    }

    pub fn set_allowed_paths(&mut self, paths: Vec<String>) {
        self.path_mapper.set_allowed_paths(paths);
    }

    pub fn main_source(&self) -> Source {
        self.source.clone()
    }

    fn get_package_root(&self, pkg: &PackageSpec) -> FileResult<PathBuf> {
        let mut manager = self.package_manager.lock().unwrap();
        manager.get_package_root(pkg)
    }

    fn get_file_content(&self, id: FileId) -> FileResult<Vec<u8>> {
        let path = match id.package() {
            Some(pkg) => {
                let root = self.get_package_root(pkg)?;
                id.vpath().resolve(&root).ok_or(FileError::AccessDenied)?
            }
            None => self.path_mapper.get_fs_file_path(id.vpath())?,
        };

        if path.is_dir() {
            return Err(FileError::IsDirectory);
        }
        std::fs::read(&path).map_err(|err| FileError::from_io(err, &path))
    }
}

impl typst::World for KatvanWorld<'_> {
    fn library(&self) -> &LazyHash<Library> {
        &self.library
    }

    fn main(&self) -> FileId {
        *MAIN_ID
    }

    fn source(&self, id: FileId) -> FileResult<Source> {
        if id == *MAIN_ID {
            return Ok(self.source.clone());
        }

        let bytes = self.get_file_content(id)?;
        let text = String::from_utf8(bytes)?;
        Ok(Source::new(id, text))
    }

    fn file(&self, id: FileId) -> FileResult<Bytes> {
        self.get_file_content(id).map(Bytes::new)
    }

    fn book(&self) -> &LazyHash<FontBook> {
        &self.book
    }

    fn font(&self, index: usize) -> Option<Font> {
        self.fonts.get(index).and_then(FontSlot::get)
    }

    fn today(&self, offset: Option<i64>) -> Option<typst::foundations::Datetime> {
        let now = self.now?;

        let in_offset = match offset {
            None => now,
            Some(offset) => {
                let hours: i8 = offset.try_into().ok()?;
                let offset = time::UtcOffset::from_hms(hours, 0, 0).ok()?;
                now.to_offset(offset)
            }
        };
        Some(typst::foundations::Datetime::Date(in_offset.date()))
    }
}

impl typst_ide::IdeWorld for KatvanWorld<'_> {
    fn upcast(&self) -> &dyn typst::World {
        self
    }

    fn packages(&self) -> &[(PackageSpec, Option<EcoString>)] {
        // TODO - Replace with OnceLock::get_or_try_init when it is stabilized
        self.packages_list
            .get_or_try_init(|| {
                let mut manager = self.package_manager.lock().unwrap();
                manager.get_all_packages()
            })
            .map(|v| &v[..])
            .unwrap_or(&[])
    }
}

struct PackageManagerWrapper<'a> {
    proxy: Pin<&'a mut ffi::PackageManagerProxy>,
    roots_cache: HashMap<PackageSpec, PathBuf>,
}

impl<'a> PackageManagerWrapper<'a> {
    fn new(proxy: Pin<&'a mut ffi::PackageManagerProxy>) -> Self {
        Self {
            proxy,
            roots_cache: HashMap::new(),
        }
    }

    fn get_package_root(&mut self, pkg: &PackageSpec) -> FileResult<PathBuf> {
        if let Some(root) = self.roots_cache.get(pkg) {
            return Ok(root.clone());
        }

        let path = self.proxy.as_mut().get_package_local_path(
            &pkg.namespace,
            &pkg.name,
            &pkg.version.to_string(),
        );

        self.check_package_manager_error(pkg)?;

        let path = PathBuf::from(path);
        self.roots_cache.insert(pkg.clone(), path.clone());
        Ok(path)
    }

    fn get_all_packages(&mut self) -> Result<Vec<(PackageSpec, Option<EcoString>)>, ()> {
        let entries = self.proxy.as_mut().get_preview_packages_listing();
        if self.proxy.error() != ffi::PackageManagerError::Success {
            return Err(());
        }

        let entries = entries
            .iter()
            .filter_map(|entry| {
                let spec = PackageSpec {
                    namespace: EcoString::from("preview"),
                    name: EcoString::from(&entry.name),
                    version: entry.version.parse().ok()?,
                };

                Some((spec, Some(EcoString::from(&entry.description))))
            })
            .collect();

        Ok(entries)
    }

    fn check_package_manager_error(&self, pkg: &PackageSpec) -> Result<(), PackageError> {
        let error = self.proxy.error();
        let message = EcoString::from(self.proxy.error_message());

        match error {
            ffi::PackageManagerError::Success => Ok(()),
            ffi::PackageManagerError::NotFound => Err(PackageError::NotFound(pkg.clone())),
            ffi::PackageManagerError::NetworkError => {
                Err(PackageError::NetworkFailed(Some(message)))
            }
            ffi::PackageManagerError::ArchiveError => {
                Err(PackageError::MalformedArchive(Some(message)))
            }
            _ => Err(PackageError::Other(Some(message))),
        }
    }
}
