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
use std::sync::Arc;

use typst::{
    foundations::Smart,
    layout::{Frame, FrameItem, GroupItem, Page, Ratio},
    text::TextItem,
    visualize::{
        Color, ConicGradient, FixedStroke, Gradient, LinearGradient, Paint, RadialGradient, Shape,
    },
};

pub fn invert_page_colors(page: &Page) -> Page {
    Page {
        frame: invert_frame_colors(&page.frame),
        fill: Smart::Custom(page.fill_or_white().as_ref().map(process_paint)),
        numbering: page.numbering.clone(),
        supplement: page.supplement.clone(),
        number: page.number,
    }
}

fn invert_frame_colors(frame: &Frame) -> Frame {
    let mut result = Frame::new(frame.size(), frame.kind());

    for (pos, item) in frame.items() {
        result.push(*pos, process_frame_item(item));
    }
    result
}

fn process_frame_item(item: &FrameItem) -> FrameItem {
    match item {
        FrameItem::Group(group) => FrameItem::Group(GroupItem {
            frame: invert_frame_colors(&group.frame),
            transform: group.transform,
            clip: group.clip.clone(),
            label: group.label,
            parent: group.parent,
        }),
        FrameItem::Text(text) => FrameItem::Text(TextItem {
            font: text.font.clone(),
            size: text.size,
            fill: process_paint(&text.fill),
            stroke: text.stroke.as_ref().map(process_stroke),
            lang: text.lang,
            region: text.region,
            text: text.text.clone(),
            glyphs: text.glyphs.clone(),
        }),
        FrameItem::Shape(shape, span) => FrameItem::Shape(process_shape(shape), *span),
        _ => item.clone(),
    }
}

fn process_shape(shape: &Shape) -> Shape {
    Shape {
        geometry: shape.geometry.clone(),
        fill: shape.fill.as_ref().map(process_paint),
        fill_rule: shape.fill_rule,
        stroke: shape.stroke.as_ref().map(process_stroke),
    }
}

fn process_stroke(stroke: &FixedStroke) -> FixedStroke {
    FixedStroke {
        paint: process_paint(&stroke.paint),
        thickness: stroke.thickness,
        cap: stroke.cap,
        join: stroke.join,
        dash: stroke.dash.clone(),
        miter_limit: stroke.miter_limit,
    }
}

fn process_paint(paint: &Paint) -> Paint {
    match paint {
        Paint::Solid(color) => Paint::Solid(process_color(color)),
        Paint::Gradient(gradient) => Paint::Gradient(process_gradient(gradient)),
        _ => paint.clone(),
    }
}

fn process_gradient(gradient: &Gradient) -> Gradient {
    match gradient {
        Gradient::Linear(value) => Gradient::Linear(Arc::new(LinearGradient {
            stops: process_gradient_stops(&value.stops),
            angle: value.angle,
            space: typst::visualize::ColorSpace::Hsl,
            relative: value.relative,
            anti_alias: value.anti_alias,
        })),
        Gradient::Radial(value) => Gradient::Radial(Arc::new(RadialGradient {
            stops: process_gradient_stops(&value.stops),
            center: value.center,
            radius: value.radius,
            focal_center: value.focal_center,
            focal_radius: value.focal_radius,
            space: typst::visualize::ColorSpace::Hsl,
            relative: value.relative,
            anti_alias: value.anti_alias,
        })),
        Gradient::Conic(value) => Gradient::Conic(Arc::new(ConicGradient {
            stops: process_gradient_stops(&value.stops),
            angle: value.angle,
            center: value.center,
            space: typst::visualize::ColorSpace::Hsl,
            relative: value.relative,
            anti_alias: value.anti_alias,
        })),
    }
}

fn process_gradient_stops(stops: &[(Color, Ratio)]) -> Vec<(Color, Ratio)> {
    stops
        .iter()
        .map(|(color, ratio)| (process_color(color), *ratio))
        .collect()
}

fn process_color(color: &Color) -> Color {
    if let Color::Hsl(mut hsl) = color.to_hsl() {
        hsl.color.lightness = 1.0 - hsl.color.lightness;
        Color::Hsl(hsl)
    } else {
        *color
    }
}
