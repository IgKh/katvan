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
use std::{
    cell::{OnceCell, RefCell},
    collections::HashMap,
    path::PathBuf,
};

use comemo::Prehashed;
use typst::{
    diag::{EcoString, FileError, FileResult, PackageError},
    foundations::Bytes,
    syntax::{package::PackageSpec, FileId, Source, VirtualPath},
    text::{Font, FontBook, FontInfo},
    Library,
};

use crate::bridge::ffi;

lazy_static::lazy_static! {
    pub static ref MAIN_ID: FileId = FileId::new_fake(VirtualPath::new("MAIN"));
}

pub struct KatvanWorld<'a> {
    root: PathBuf,
    package_manager: &'a ffi::PackageManagerProxy,
    package_roots: RefCell<HashMap<PackageSpec, PathBuf>>,
    library: Prehashed<Library>,
    fonts: FontManager,
    source: Source,
    now: Option<time::OffsetDateTime>,
}

impl<'a> KatvanWorld<'a> {
    pub fn new(package_manager: &'a ffi::PackageManagerProxy, root: &str) -> Self {
        let fonts = FontManager::new();
        let source = Source::new(*MAIN_ID, String::new());

        KatvanWorld {
            root: PathBuf::from(root),
            package_manager,
            package_roots: RefCell::new(HashMap::new()),
            library: Prehashed::new(Library::default()),
            fonts,
            source,
            now: time::OffsetDateTime::now_local().ok(),
        }
    }

    pub fn reset_source(&mut self, text: &str) {
        self.source.replace(text);
        self.now = time::OffsetDateTime::now_local().ok();
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

    fn get_package_root(&self, pkg: Option<&PackageSpec>) -> FileResult<PathBuf> {
        if let Some(pkg) = pkg {
            let mut cache = self.package_roots.borrow_mut();

            let root = match cache.get(pkg) {
                Some(root) => root.clone(),
                None => {
                    let path = self.package_manager.get_package_local_path(
                        &pkg.namespace,
                        &pkg.name,
                        &pkg.version.to_string(),
                    );

                    self.check_package_manager_error(pkg)?;

                    let path = PathBuf::from(path);
                    cache.insert(pkg.clone(), path.clone());
                    path
                }
            };
            Ok(root)
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
    fn library(&self) -> &Prehashed<Library> {
        &self.library
    }

    fn main(&self) -> Source {
        self.source.clone()
    }

    fn source(&self, id: FileId) -> FileResult<Source> {
        if id == *MAIN_ID {
            return Ok(self.main());
        }

        let bytes = self.get_file_content(id)?;
        let text = String::from_utf8(bytes)?;
        Ok(Source::new(id, text))
    }

    fn file(&self, id: FileId) -> FileResult<Bytes> {
        self.get_file_content(id).map(Bytes::from)
    }

    fn book(&self) -> &Prehashed<FontBook> {
        &self.fonts.book
    }

    fn font(&self, index: usize) -> Option<Font> {
        self.fonts.font(index)
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

struct FontFace {
    id: fontdb::ID,
    font: OnceCell<Option<Font>>,
}

struct FontManager {
    db: fontdb::Database,
    book: Prehashed<FontBook>,
    faces: Vec<FontFace>,
}

impl FontManager {
    fn new() -> Self {
        let mut db = fontdb::Database::new();

        db.load_system_fonts();
        for data in typst_assets::fonts() {
            db.load_font_data(data.into());
        }

        let mut book = FontBook::new();
        let mut faces: Vec<FontFace> = Vec::new();

        db.faces().for_each(|f| {
            db.with_face_data(f.id, |data, index| {
                let info = FontInfo::new(data, index);
                if let Some(info) = info {
                    book.push(info);
                    faces.push(FontFace {
                        id: f.id,
                        font: OnceCell::new(),
                    });
                }
            });
        });

        FontManager {
            db,
            book: Prehashed::new(book),
            faces,
        }
    }

    fn font(&self, index: usize) -> Option<Font> {
        self.faces.get(index).and_then(|entry| {
            entry
                .font
                .get_or_init(|| {
                    self.db
                        .with_face_data(entry.id, |data, index| Font::new(data.into(), index))
                        .flatten()
                })
                .clone()
        })
    }
}
