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
use std::sync::LazyLock;

use typst::{
    Library, LibraryExt, World,
    diag::FileResult,
    syntax::{FileId, Source, VirtualPath},
    text::FontBook,
    utils::LazyHash,
};
use typst_ide::IdeWorld;
use typst_kit::fonts::{FontSlot, Fonts};

static TEST_ID: LazyLock<FileId> = LazyLock::new(|| FileId::new_fake(VirtualPath::new("TEST")));

pub(crate) struct TestWorld {
    library: LazyHash<Library>,
    book: LazyHash<FontBook>,
    fonts: Vec<FontSlot>,
    source: Source,
}

impl TestWorld {
    pub fn new(content: &str) -> Self {
        let fonts = Fonts::searcher()
            .include_embedded_fonts(true)
            .include_system_fonts(false)
            .search();

        let source = Source::new(*TEST_ID, String::from(content));

        Self {
            library: LazyHash::new(Library::default()),
            book: LazyHash::new(fonts.book),
            fonts: fonts.fonts,
            source,
        }
    }
}

impl World for TestWorld {
    fn library(&self) -> &LazyHash<Library> {
        &self.library
    }

    fn book(&self) -> &LazyHash<FontBook> {
        &self.book
    }

    fn main(&self) -> FileId {
        *TEST_ID
    }

    fn source(&self, _id: FileId) -> FileResult<Source> {
        Ok(self.source.clone())
    }

    fn file(&self, _id: FileId) -> FileResult<typst::foundations::Bytes> {
        Err(typst::diag::FileError::AccessDenied)
    }

    fn font(&self, index: usize) -> Option<typst::text::Font> {
        self.fonts.get(index).and_then(FontSlot::get)
    }

    fn today(&self, _offset: Option<i64>) -> Option<typst::foundations::Datetime> {
        None
    }
}

impl IdeWorld for TestWorld {
    fn upcast(&self) -> &dyn World {
        self
    }
}
