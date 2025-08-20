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
#import "macshell_editorview.h"

#include <QMenu>

@interface KatvanEditorView ()

@property katvan::Editor* editor;

@end

@implementation KatvanEditorView

- (instancetype)initWithDocument:(katvan::Document*)textDocument
{
    self = [super init];
    if (self) {
        self.editor = new katvan::Editor(textDocument);
    }
    return self;
}

- (void)dealloc
{
    delete self.editor;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    NSView* editorView = (__bridge NSView *)reinterpret_cast<void*>(self.editor->winId());

    [self.view addSubview:editorView];

    editorView.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
        [editorView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [editorView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [editorView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [editorView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumHeight()],
    ]];

    self.editor->show();
}

- (NSMenu*)createInsertMenu
{
    // FIXME Does this leak?
    return self.editor->createInsertMenu()->toNSMenu();
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(performUndo:)) {
        return self.editor->document()->isUndoAvailable();
    }
    if (action == @selector(performRedo:)) {
        return self.editor->document()->isRedoAvailable();
    }
    if (action == @selector(cut:) || action == @selector(copy:) || action == @selector(delete:)) {
        QTextCursor cursor = self.editor->textCursor();
        return cursor.hasSelection();
    }
    if (action == @selector(paste:)) {
        return self.editor->canPaste();
    }
    return YES;
}

- (void)performUndo:(id)sender
{
    self.editor->undo();
}

- (void)performRedo:(id)sender
{
    self.editor->redo();
}

- (void)cut:(id)sender
{
    self.editor->cut();
}

- (void)copy:(id)sender
{
    self.editor->copy();
}

- (void)paste:(id)sender
{
    self.editor->paste();
}

- (void)delete:(id)sender
{
    QTextCursor cursor = self.editor->textCursor();
    cursor.removeSelectedText();
}

- (void)selectAll:(id)sender
{
    self.editor->selectAll();
}

- (void)zoomFontToActualSize:(id)sender
{
    self.editor->resetFontSize();
}

- (void)zoomInFont:(id)sender
{
    self.editor->increaseFontSize();
}

- (void)zoomOutFont:(id)sender
{
    self.editor->decreaseFontSize();
}

@end
