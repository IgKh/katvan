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
    path::{Component, Path, PathBuf},
    pin::Pin,
    sync::{LazyLock, Mutex},
};

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

pub static MAIN_ID: LazyLock<FileId> = LazyLock::new(|| {
    FileId::new_fake(VirtualPath::new("MAIN"))
});

pub struct KatvanWorld<'a> {
    root: PathBuf,
    package_roots: Mutex<PackageRoots<'a>>,
    allowed_paths: Vec<PathBuf>,
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
            allowed_paths: Vec::new(),
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

    pub fn discard_package_roots_cache(&mut self) {
        let mut roots = self.package_roots.lock().unwrap();
        roots.cache.clear();
    }

    pub fn set_allowed_paths(&mut self, paths: Vec<String>) {
        self.allowed_paths = paths
            .into_iter()
            .map(Into::into)
            .filter(|p: &PathBuf| p.is_absolute())
            .collect();
    }

    pub fn main_source(&self) -> Source {
        self.source.clone()
    }

    fn get_package_root(&self, pkg: &PackageSpec) -> FileResult<PathBuf> {
        let mut roots = self.package_roots.lock().unwrap();
        roots.get(pkg)
    }

    fn get_fs_file_path(&self, path: &VirtualPath) -> FileResult<PathBuf> {
        if self.root.as_os_str().is_empty() {
            return Err(FileError::Other(Some(EcoString::from(
                "unsaved files cannot include external files",
            ))));
        }

        let path = join_and_normalize_path(&self.root, path.as_rootless_path());

        let allowed_roots = std::iter::once(&self.root).chain(self.allowed_paths.iter());
        for root in allowed_roots {
            if path.starts_with(root) {
                return Ok(path);
            }
        }
        Err(FileError::AccessDenied)
    }

    fn get_file_content(&self, id: FileId) -> FileResult<Vec<u8>> {
        let path = match id.package() {
            Some(pkg) => {
                let root = self.get_package_root(pkg)?;
                id.vpath().resolve(&root).ok_or(FileError::AccessDenied)?
            }
            None => self.get_fs_file_path(id.vpath())?,
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

impl typst_ide::IdeWorld for KatvanWorld<'_> {
    fn upcast(&self) -> &dyn typst::World {
        self
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
            ffi::PackageManagerError::ArchiveError => {
                Err(PackageError::MalformedArchive(Some(message)))
            }
            _ => Err(PackageError::Other(Some(message))),
        }
    }
}

fn join_and_normalize_path(base: &Path, path: &Path) -> PathBuf {
    let mut result = base.to_path_buf();

    for component in path.components() {
        match component {
            Component::CurDir | Component::Prefix(_) => {}
            Component::ParentDir => {
                result.pop();
            }
            _ => result.push(component),
        }
    }

    result
}
