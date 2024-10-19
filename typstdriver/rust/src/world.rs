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
use std::{collections::HashMap, path::PathBuf, pin::Pin, sync::Mutex};

use time::{format_description::well_known::Iso8601, OffsetDateTime};
use typst::{
    diag::{EcoString, FileError, FileResult, PackageError},
    foundations::Bytes,
    syntax::{package::PackageSpec, FileId, Source, VirtualPath},
    text::{Font, FontBook},
    utils::LazyHash,
    Library,
};
use typst_kit::fonts::{FontSlot, Fonts};

use crate::bridge::ffi;

lazy_static::lazy_static! {
    pub static ref MAIN_ID: FileId = FileId::new_fake(VirtualPath::new("MAIN"));
}

pub struct KatvanWorld<'a> {
    root: PathBuf,
    package_roots: Mutex<PackageRoots<'a>>,
    library: LazyHash<Library>,
    book: LazyHash<FontBook>,
    fonts: Vec<FontSlot>,
    source: Source,
    now: Option<OffsetDateTime>,
}

impl<'a> KatvanWorld<'a> {
    pub fn new(package_manager: Pin<&'a mut ffi::PackageManagerProxy>, root: &str) -> Self {
        let fonts = Fonts::searcher().search();
        let source = Source::new(*MAIN_ID, String::new());

        KatvanWorld {
            root: PathBuf::from(root),
            package_roots: Mutex::new(PackageRoots::new(package_manager)),
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
        let from = self.source.utf16_to_byte(from_utf16_idx);
        let to = self.source.utf16_to_byte(to_utf16_idx);

        if let (Some(from), Some(to)) = (from, to) {
            self.source.edit(from..to, text);
        }
    }

    pub fn reset_current_date(&mut self, now: &str) {
        self.now = OffsetDateTime::parse(now, &Iso8601::PARSING).ok();
    }

    pub fn main_source(&self) -> Source {
        self.source.clone()
    }

    fn get_package_root(&self, pkg: Option<&PackageSpec>) -> FileResult<PathBuf> {
        if let Some(pkg) = pkg {
            let mut roots = self.package_roots.lock().unwrap();
            roots.get(pkg)
        } else {
            if self.root.as_os_str().is_empty() {
                return Err(FileError::Other(Some(EcoString::from(
                    "unsaved files cannot include external files",
                ))));
            }
            Ok(self.root.clone())
        }
    }

    fn get_file_content(&self, id: FileId) -> FileResult<Vec<u8>> {
        let root = self.get_package_root(id.package())?;
        let path = id.vpath().resolve(&root).ok_or(FileError::AccessDenied)?;

        if path.is_dir() {
            return Err(FileError::IsDirectory);
        }
        std::fs::read(&path).map_err(|err| FileError::from_io(err, &path))
    }
}

impl<'a> typst::World for KatvanWorld<'a> {
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
        self.get_file_content(id).map(Bytes::from)
    }

    fn book(&self) -> &LazyHash<FontBook> {
        &self.book
    }

    fn font(&self, index: usize) -> Option<Font> {
        self.fonts.get(index).and_then(|slot| slot.get())
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

struct PackageRoots<'a> {
    package_manager: Pin<&'a mut ffi::PackageManagerProxy>,
    cache: HashMap<PackageSpec, PathBuf>,
}

impl<'a> PackageRoots<'a> {
    fn new(package_manager: Pin<&'a mut ffi::PackageManagerProxy>) -> Self {
        PackageRoots {
            package_manager,
            cache: HashMap::new(),
        }
    }

    fn get(&mut self, pkg: &PackageSpec) -> FileResult<PathBuf> {
        let root = match self.cache.get(pkg) {
            Some(root) => root.clone(),
            None => {
                let path = self.package_manager.as_mut().get_package_local_path(
                    &pkg.namespace,
                    &pkg.name,
                    &pkg.version.to_string(),
                );

                self.check_package_manager_error(pkg)?;

                let path = PathBuf::from(path);
                self.cache.insert(pkg.clone(), path.clone());
                path
            }
        };
        Ok(root)
    }

    fn check_package_manager_error(&self, pkg: &PackageSpec) -> Result<(), PackageError> {
        let error = self.package_manager.error();
        let message = EcoString::from(self.package_manager.error_message());

        match error {
            ffi::PackageManagerError::Success => Ok(()),
            ffi::PackageManagerError::NotFound => Err(PackageError::NotFound(pkg.clone())),
            ffi::PackageManagerError::NetworkError => {
                Err(PackageError::NetworkFailed(Some(message)))
            }
            ffi::PackageManagerError::IoError => Err(PackageError::Other(Some(message))),
            ffi::PackageManagerError::ArchiveError => {
                Err(PackageError::MalformedArchive(Some(message)))
            }
            _ => Err(PackageError::Other(Some(message))),
        }
    }
}
