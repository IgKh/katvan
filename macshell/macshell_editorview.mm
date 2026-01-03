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
#import "macshell_editorview.h"
#import "macshell_textfinderclient.h"

#include <QInputDialog>
#include <QMenu>

@interface KatvanEditorView ()

@property (nonatomic) katvan::Editor* editor;
@property (nonatomic) QInputDialog* goToLineDialog;

@property (nonatomic) NSTextFinder* textFinder;
@property (nonatomic) KatvanTextFinderClient* textFinderClient;
@property (nonatomic) NSView* findBarContainerView;
@property (nonatomic) NSLayoutConstraint* findBarContainerHeightConstraint;
@end

@implementation KatvanEditorView {
    CGFloat d_findBarHeight;
}

- (instancetype)initWithDocument:(katvan::Document*)textDocument
{
    self = [super init];
    if (self) {
        self.identifier = [self className];
        self.editor = new katvan::Editor(textDocument);
        self.goToLineDialog = nullptr;

        self.textFinderClient = [[KatvanTextFinderClient alloc] initWithEditor:self.editor];
        self.textFinder = [[NSTextFinder alloc] init];
        self.textFinder.client = self.textFinderClient;
        self.textFinder.findBarContainer = self;

        __weak __typeof__(self) weakSelf = self;

        QObject::connect(self.editor, &QTextEdit::cursorPositionChanged,
                         self.editor, [weakSelf]() {
            [weakSelf invalidateRestorableState];
        });
    }
    return self;
}

- (void)dealloc
{
    [self.view removeObserver:self forKeyPath:@"effectiveAppearance"];

    delete self.editor;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    [self.view addObserver:self forKeyPath:@"effectiveAppearance" options:0 context:nil];

    NSView* editorView = (__bridge NSView *)reinterpret_cast<void*>(self.editor->winId());
    editorView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:editorView];

    self.findBarContainerView = [[NSView alloc] init];
    self.findBarContainerView.hidden = YES;
    self.findBarContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.findBarContainerView];

    self.findBarContainerHeightConstraint = [self.findBarContainerView.heightAnchor constraintEqualToConstant:0];

    [NSLayoutConstraint activateConstraints:@[
        [self.findBarContainerView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.findBarContainerView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.findBarContainerView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [editorView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [editorView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [editorView.topAnchor constraintEqualToAnchor:self.findBarContainerView.bottomAnchor],
        [editorView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumHeight()],
        self.findBarContainerHeightConstraint,
    ]];

    self.editor->show();
}

- (void)restoreStateWithCoder:(NSCoder*)coder
{
    // Decode text cursor position
    if ([coder containsValueForKey:@"cursorPos"]) {
        NSInteger pos = [coder decodeIntegerForKey:@"cursorPos"];

        if (pos >= 0 && pos <= self.editor->document()->characterCount()) {
            QTextCursor cursor { self.editor->document() };
            cursor.setPosition(pos);
            self.editor->setTextCursor(cursor);
        }
    }

    [super restoreStateWithCoder:coder];
}

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder
{
    // Encode text cursor position
    NSInteger cursorPos = self.editor->textCursor().position();
    [coder encodeInteger:cursorPos forKey:@"cursorPos"];

    [super encodeRestorableStateWithCoder:coder];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        self.editor->updateEditorTheme();
    }
    else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (NSMenu*)createInsertMenu
{
    return self.editor->createInsertMenu()->toNSMenu();
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(performTextFinderAction:)) {
        return [self.textFinder validateAction:(NSTextFinderAction)[item tag]];
    }
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
    if (action == @selector(goBack:)) {
        return self.editor->isGoBackAvailable();
    }
    if (action == @selector(goForward:)) {
        return self.editor->isGoForwardAvailable();
    }
    return YES;
}

- (void)performTextFinderAction:(id)sender
{
    NSTextFinderAction action = (NSTextFinderAction)[sender tag];
    if (action == NSTextFinderActionShowFindInterface || action == NSTextFinderActionShowReplaceInterface) {
        [self setFindBarVisible:YES];
    }
    [self.textFinder performAction:action];
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

- (void)zoomToActualSize:(id)sender
{
    self.editor->resetFontSize();
}

- (void)zoomIn:(id)sender
{
    self.editor->increaseFontSize();
}

- (void)zoomOut:(id)sender
{
    self.editor->decreaseFontSize();
}

- (void)goBack:(id)sender
{
    self.editor->goBack();
}

- (void)goForward:(id)sender
{
    self.editor->goForward();
}

- (void)goToLine:(id)sender
{
    if (!self.goToLineDialog) {
        self.goToLineDialog = new QInputDialog(self.editor, Qt::Sheet);
        self.goToLineDialog->setInputMode(QInputDialog::IntInput);

        __weak __typeof__(self) weakSelf = self;
        QObject::connect(self.goToLineDialog, &QInputDialog::accepted,
                        self.goToLineDialog, [weakSelf]() {
            int lineNumber = weakSelf.goToLineDialog->intValue();
            weakSelf.editor->goToBlock(lineNumber - 1, 0);
        });
    }

    int lineCount = self.editor->document()->blockCount();
    NSString* msg = [NSString stringWithFormat:NSLocalizedString(@"Enter a line number (up to %d):", nil), lineCount];

    self.goToLineDialog->setLabelText(QString::fromNSString(msg));
    self.goToLineDialog->setIntRange(1, lineCount);
    self.goToLineDialog->open();
}

- (void)showColorPicker
{
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    [panel setContinuous:NO];
    [panel makeKeyAndOrderFront:self];
}

//
// NSTextFinderBarContainer protocol methods
//

- (NSView*)findBarView
{
    return self.findBarContainerView.subviews.firstObject;
}

- (void)setFindBarView:(NSView*)view
{
    for (NSView* subview in self.findBarContainerView.subviews) {
        [subview removeFromSuperview];
    }

    if (view) {
        [self.findBarContainerView addSubview:view];

        view.translatesAutoresizingMaskIntoConstraints = NO;
        [NSLayoutConstraint activateConstraints:@[
            [view.leadingAnchor constraintEqualToAnchor:self.findBarContainerView.leadingAnchor],
            [view.trailingAnchor constraintEqualToAnchor:self.findBarContainerView.trailingAnchor],
            [view.topAnchor constraintEqualToAnchor:self.findBarContainerView.topAnchor],
            [view.bottomAnchor constraintEqualToAnchor:self.findBarContainerView.bottomAnchor],
        ]];
    }
}

- (BOOL)isFindBarVisible
{
    return !self.findBarContainerView.hidden;
}

- (void)setFindBarVisible:(BOOL)visible
{
    self.findBarContainerView.hidden = !visible;
    if (self.findBarView && visible) {
        self.findBarContainerHeightConstraint.constant = d_findBarHeight;
    }
    else {
        self.findBarContainerHeightConstraint.constant = 0;
    }
}

- (void) findBarViewDidChangeHeight
{
    d_findBarHeight = self.findBarView ? self.findBarView.frame.size.height : 0;
    self.findBarContainerHeightConstraint.constant = d_findBarHeight;
}

@end
