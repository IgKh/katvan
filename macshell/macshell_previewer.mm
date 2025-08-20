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
#import "macshell_previewer.h"

@interface KatvanPreviewer ()

@property katvan::PreviewerView* previewerView;

@end

@implementation KatvanPreviewer

- (instancetype)initWithDriver:(katvan::TypstDriverWrapper *)driver
{
    self = [super init];
    if (self) {
        self.previewerView = new katvan::PreviewerView(driver);

        QObject::connect(driver, &katvan::TypstDriverWrapper::previewReady, self.previewerView, &katvan::PreviewerView::setPages);
    }
    return self;
}

- (void)dealloc
{
    delete self.previewerView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    NSView* previewerNsView = (__bridge NSView *)reinterpret_cast<void*>(self.previewerView->winId());

    [self.view addSubview:previewerNsView];

    previewerNsView.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
        [previewerNsView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [previewerNsView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [previewerNsView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [previewerNsView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumHeight()],
    ]];

    self.previewerView->show();
}

- (qreal)currentRoundZoomFactor
{
    qreal factor = self.previewerView->effectiveZoom(self.previewerView->currentPage());
    return qRound(factor * 20) / 20.0; // Round to nearest multiple of 5%
}

- (void)zoomIn:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] + 0.05;
    self.previewerView->setCustomZoomFactor(factor);
}

- (void)zoomOut:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] - 0.05;
    self.previewerView->setCustomZoomFactor(factor);
}

@end
