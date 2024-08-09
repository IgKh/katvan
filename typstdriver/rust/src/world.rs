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
use std::cell::OnceCell;

use comemo::Prehashed;
use typst::{
    diag::FileResult,
    foundations::Bytes,
    syntax::{FileId, Source, VirtualPath},
    text::{Font, FontBook, FontInfo},
    Library,
};
use lazy_static::lazy_static;

lazy_static! {
    static ref MAIN_ID: FileId = FileId::new_fake(VirtualPath::new("MAIN"));
}

pub struct KatvanWorld {
    library: Prehashed<Library>,
    fonts: FontManager,
    source: Source,
    now: Option<time::OffsetDateTime>,
}

impl KatvanWorld {
    pub fn new() -> Self {
        let fonts = FontManager::new();

        let source = Source::new(*MAIN_ID, String::new());

        KatvanWorld {
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
}

impl typst::World for KatvanWorld {
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

        // TODO
        Err(typst::diag::FileError::InvalidUtf8)
    }

    fn file(&self, _id: FileId) -> FileResult<Bytes> {
        // TODO
        Err(typst::diag::FileError::InvalidUtf8)
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
