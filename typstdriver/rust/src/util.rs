/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2026 Igor Khanin
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
use std::sync::LazyLock;

use typst::{
    Library, LibraryExt, World,
    diag::FileResult,
    syntax::{FileId, RootedPath, Source, VirtualPath, VirtualRoot},
    text::FontBook,
    utils::LazyHash,
};
use typst_ide::IdeWorld;
use typst_kit::fonts::FontStore;

static SOURCE_ID: LazyLock<FileId> = LazyLock::new(|| {
    FileId::unique(RootedPath::new(
        VirtualRoot::Project,
        VirtualPath::new("TEST").unwrap(),
    ))
});

pub struct AuxWorld {
    library: LazyHash<Library>,
    fonts: FontStore,
    source: Source,
}

impl AuxWorld {
    pub fn new(content: &str) -> Self {
        let mut fonts = FontStore::new();
        fonts.extend(typst_kit::fonts::embedded());

        let source = Source::new(*SOURCE_ID, String::from(content));

        let library = Library::builder()
            .with_features(typst::Features::all())
            .build();

        Self {
            library: LazyHash::new(library),
            fonts,
            source,
        }
    }
}

impl World for AuxWorld {
    fn library(&self) -> &LazyHash<Library> {
        &self.library
    }

    fn book(&self) -> &LazyHash<FontBook> {
        self.fonts.book()
    }

    fn main(&self) -> FileId {
        *SOURCE_ID
    }

    fn source(&self, _id: FileId) -> FileResult<Source> {
        Ok(self.source.clone())
    }

    fn file(&self, _id: FileId) -> FileResult<typst::foundations::Bytes> {
        Err(typst::diag::FileError::AccessDenied)
    }

    fn font(&self, index: usize) -> Option<typst::text::Font> {
        self.fonts.font(index)
    }

    fn today(
        &self,
        _offset: Option<typst::foundations::Duration>,
    ) -> Option<typst::foundations::Datetime> {
        None
    }
}

impl IdeWorld for AuxWorld {
    fn upcast(&self) -> &dyn World {
        self
    }
}
