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
use std::path::{Component, Path, PathBuf};

use typst::{
    diag::{FileError, FileResult},
    ecow::EcoString,
    syntax::VirtualPath,
};

pub struct PathMapper {
    root: PathMapEntry,
    allowed_paths: Vec<PathMapEntry>,
}

impl PathMapper {
    pub fn new(root: &str) -> Self {
        Self {
            root: PathMapEntry::new(root),
            allowed_paths: Vec::new(),
        }
    }

    pub fn set_allowed_paths(&mut self, paths: Vec<String>) {
        self.allowed_paths = paths
            .into_iter()
            .map(PathMapEntry::new)
            .filter(|e| e.actual.is_absolute())
            .collect();
    }

    pub fn get_fs_file_path(&self, path: &VirtualPath) -> FileResult<PathBuf> {
        if self.root.actual.as_os_str().is_empty() {
            return Err(FileError::Other(Some(EcoString::from(
                "unsaved files cannot include external files",
            ))));
        }

        let displayed_path = join_and_normalize_path(&self.root.displayed, path.as_rootless_path());

        let allowed_roots = std::iter::once(&self.root).chain(self.allowed_paths.iter());
        for root in allowed_roots {
            if let Ok(path_in_root) = displayed_path.strip_prefix(&root.displayed) {
                let actual_path = root.actual.join(path_in_root);
                if root.actual != root.displayed && !actual_path.exists() {
                    continue;
                }
                return Ok(root.actual.join(path_in_root));
            }
        }
        Err(FileError::AccessDenied)
    }
}

struct PathMapEntry {
    actual: PathBuf,
    displayed: PathBuf,
}

impl PathMapEntry {
    fn new<S: AsRef<str>>(path: S) -> Self {
        let actual = PathBuf::from(path.as_ref());
        let displayed = get_display_path(&actual);

        Self { actual, displayed }
    }
}

#[cfg(feature = "flatpak")]
pub fn get_display_path<P: AsRef<Path>>(path: P) -> PathBuf {
    use std::ffi::OsString;
    use std::os::unix::ffi::OsStringExt;

    match xattr::get(path.as_ref(), "user.document-portal.host-path")
        .ok()
        .flatten()
    {
        Some(buf) => OsString::from_vec(buf).into(),
        None => path.as_ref().to_owned(),
    }
}

#[cfg(not(feature = "flatpak"))]
pub fn get_display_path<P: AsRef<Path>>(path: P) -> PathBuf {
    path.as_ref().to_owned()
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
