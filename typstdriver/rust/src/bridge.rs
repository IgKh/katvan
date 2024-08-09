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
use crate::engine::EngineImpl;

#[cxx::bridge(namespace = "katvan::typstdriver")]
pub(crate) mod ffi {
    unsafe extern "C++" {
        include!("typstdriver_logger_p.h");

        type RustLogger;

        #[rust_name = "log_one"]
        fn logOne(&self, message: &str);

        #[rust_name = "log_to_batch"]
        fn logToBatch(&self, message: &str);

        #[rust_name = "release_batched"]
        fn releaseBatched(&self);
    }

    extern "Rust" {
        type EngineImpl<'a>;

        fn compile(&mut self, source: &str) -> Vec<u8>;

        unsafe fn create_engine_impl<'a>(logger: &'a RustLogger, instance_id: &str) -> Box<EngineImpl<'a>>;
    }
}

fn create_engine_impl<'a>(logger: &'a ffi::RustLogger, instance_id: &str) -> Box<EngineImpl<'a>> {
    Box::new(EngineImpl::new(logger, instance_id))
}
