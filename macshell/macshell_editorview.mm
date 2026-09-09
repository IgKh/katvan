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
#import "macshell_spellchecker.h"
#import "macshell_textfinderclient.h"
#import "macshell_widgets.h"

#include "katvan_completionmanager.h"
#include "katvan_highlighter.h"

#include <QInputDialog>
#include <QMenu>

@interface KatvanEditorView ()

@property (nonatomic) katvan::Editor* editor;
@property (nonatomic) KatvanMacSpellChecker* spellChecker;
@property (nonatomic) QInputDialog* goToLineDialog;
@property (nonatomic) KatvanEditorStatusBar* statusBar;

@property (nonatomic) NSTextFinder* textFinder;
@property (nonatomic) KatvanTextFinderClient* textFinderClient;
@property (nonatomic) NSView* findBarContainerView;
@property (nonatomic) NSLayoutConstraint* findBarContainerHeightConstraint;

@property (nonatomic, weak) NSView* editorNsView;

@end

@implementation KatvanEditorView
{
    CGFloat d_findBarHeight;
}

- (instancetype)initWithDocument:(katvan::Document*)textDocument
{
    self = [super init];
    if (self) {
        self.identifier = [self className];
        self.spellChecker = new KatvanMacSpellChecker;
        self.editor = new katvan::Editor(textDocument, self.spellChecker);
        self.goToLineDialog = nullptr;

        self.textFinderClient = [[KatvanTextFinderClient alloc] initWithEditor:self.editor];
        self.textFinder = [[NSTextFinder alloc] init];
        self.textFinder.client = self.textFinderClient;
        self.textFinder.findBarContainer = self;

        __weak __typeof__(self) weakSelf = self;

        QObject::connect(self.editor, &QTextEdit::cursorPositionChanged,
                         self.editor, [weakSelf]() {
            [weakSelf invalidateRestorableState];
            [weakSelf.statusBar updateCursorPosition:weakSelf.editor->textCursor()];
        });
    }
    return self;
}

- (void)dealloc
{
    delete self.editor;
    delete self.spellChecker;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    //self.editor->setWindowFlags(self.editor->windowFlags() | Qt::SubWindow);

    self.editorNsView = (__bridge NSView *)reinterpret_cast<void*>(self.editor->winId());
    self.editorNsView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.editorNsView];

    KatvanAuxToolBar* toolbar = [self makeEditorToolbar];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:toolbar];

    self.findBarContainerView = [NSView new];
    self.findBarContainerView.hidden = YES;
    self.findBarContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.findBarContainerView];

    self.statusBar = [[KatvanEditorStatusBar alloc] init];
    self.statusBar.delegate = self;
    self.statusBar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.statusBar];

    self.findBarContainerHeightConstraint = [self.findBarContainerView.heightAnchor constraintEqualToConstant:0];

    [NSLayoutConstraint activateConstraints:@[
        [toolbar.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [toolbar.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [toolbar.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.findBarContainerView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.findBarContainerView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.findBarContainerView.topAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
        [self.editorNsView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.editorNsView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.editorNsView.topAnchor constraintEqualToAnchor:self.findBarContainerView.bottomAnchor],
        [self.editorNsView.bottomAnchor constraintEqualToAnchor:self.statusBar.topAnchor],
        [self.statusBar.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.statusBar.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.statusBar.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.editor->minimumHeight()],
        self.findBarContainerHeightConstraint,
    ]];

    self.editor->show();
}

- (KatvanAuxToolBar*)makeEditorToolbar
{
    KatvanAuxToolBar* toolbar = [[KatvanAuxToolBar alloc] init];
    [toolbar addButtonWithIcon:[NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:@"Plus sign"]
             toolTip:NSLocalizedString(@"Insert", "Opens insert menu for the editor")
             inGravity:NSStackViewGravityTrailing
             target:self
             action:@selector(showInsertMenu:)];
    return toolbar;
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

    // Decode cursor move style
    if ([coder containsValueForKey:@"cursorMoveStyle"]) {
        auto cursorMoveStyle = (KatvanCursorMoveStyle)[coder decodeIntegerForKey:@"cursorMoveStyle"];
        self.statusBar.cursorMoveStyle = cursorMoveStyle;
    }

    [super restoreStateWithCoder:coder];
}

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder
{
    // Encode text cursor position
    NSInteger cursorPos = self.editor->textCursor().position();
    [coder encodeInteger:cursorPos forKey:@"cursorPos"];

    // Encode cursor move style
    NSInteger cursorMoveStyle = (NSInteger)self.statusBar.cursorMoveStyle;
    [coder encodeInteger:cursorMoveStyle forKey:@"cursorMoveStyle"];

    [super encodeRestorableStateWithCoder:coder];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
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

- (void)showInsertMenu:(id)sender
{
    [self ensureFocused];

    NSMenu* menu = self.editor->createInsertMenu()->toNSMenu();
    [menu setAutoenablesItems:NO];
    [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, 0) inView:sender];
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

- (void)triggerAutocomplete:(id)sender
{
    self.editor->completionManager()->startExplicitCompletion();
}

- (void)showGuessPanel:(id)sender
{
    NSPanel* panel = [[NSSpellChecker sharedSpellChecker] spellingPanel];
    [panel orderFront:self];
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
    NSString* format = NSLocalizedString(@"Enter a line number (up to %d):", "Go to line dialog prompt");
    NSString* msg = [NSString stringWithFormat:format, lineCount];

    self.goToLineDialog->setLabelText(QString::fromNSString(msg));
    self.goToLineDialog->setIntRange(1, lineCount);
    self.goToLineDialog->open();
}

- (void)resetAppearance
{
    self.editor->updateEditorTheme();
}

- (void)ensureFocused
{
    [self.view.window makeFirstResponder:self.editorNsView];
}

- (void)showColorPicker
{
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    [panel setContinuous:NO];
    [panel orderFront:self];
}

- (void)updateWordCount:(NSUInteger)count
{
    [self.statusBar updateWordCount:count];
}

//
// KatvanEditorStatusBarDelegate protocol methods
//

- (void)cursorMovementStyleChanged:(KatvanCursorMoveStyle)style
{
    if (style == KatvanCursorMoveStyleLogical) {
        self.editor->document()->setDefaultCursorMoveStyle(Qt::LogicalMoveStyle);
    }
    else {
        self.editor->document()->setDefaultCursorMoveStyle(Qt::VisualMoveStyle);
    }
    [self invalidateRestorableState];
}

//
// Spelling related methods
//

static QTextCursor findMisspellingFromCursor(QTextCursor from)
{
    for (QTextBlock block = from.block(); block.isValid(); block = block.next()) {
        auto* data = katvan::BlockData::get<katvan::SpellingBlockData>(block);
        auto words = data->misspelledWords();
        if (words.empty()) {
            continue;
        }

        for (auto& word : words) {
            int startPos = word.startPos;
            if (block == from.block() && startPos < from.positionInBlock()) {
                continue;
            }

            QTextCursor result{ block };
            result.setPosition(block.position() + startPos);
            result.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, word.length);

            return result;
        }
    }
    return QTextCursor{};
}

- (void)checkSpelling:(id)sender
{
    // Not actually documented anywhere, but the spelling panel sends the
    // checkSpelling: message to the first responder when the "Find Next"
    // button is clicked. The responder is supposed to run the spell check
    // to find the first misspelled word and update the panel. We check
    // spelling continuously, so look in the block data of the document
    // for the first one after the cursor, select it and notify the panel.
    QTextCursor cursor = self.editor->textCursor();

    QTextCursor misspelledCursor = findMisspellingFromCursor(cursor);
    if (misspelledCursor.isNull() && cursor.position() > 0) {
        // Look before the current cursor
        QTextCursor documentCursor { self.editor->document() };
        misspelledCursor = findMisspellingFromCursor(documentCursor);
    }

    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];

    if (misspelledCursor.isNull()) {
        [checker updateSpellingPanelWithMisspelledWord:@""];
    }
    else {
        self.editor->setTextCursor(misspelledCursor);
        [checker updateSpellingPanelWithMisspelledWord:misspelledCursor.selectedText().toNSString()];
    }
}

- (void)changeSpelling:(id)sender
{
    NSString* replacement = [[sender selectedCell] stringValue];

    QTextCursor cursor = self.editor->textCursor();
    cursor.insertText(QString::fromNSString(replacement));
}

- (void)ignoreSpelling:(id)sender
{
    NSString* ignored = [[sender selectedCell] stringValue];

    self.spellChecker->ignoreWord(ignored);
    self.editor->forceRehighlighting();
}

//
// NSTextFinderBarContainer protocol methods
//

- (NSView*)contentView
{
    return self.editorNsView;
}

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

- (void)findBarViewDidChangeHeight
{
    d_findBarHeight = self.findBarView ? self.findBarView.frame.size.height : 0;
    self.findBarContainerHeightConstraint.constant = d_findBarHeight;
}

@end
