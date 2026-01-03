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
#import "macshell_textfinderclient.h"

#include "katvan_editorlayout.h"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>

@interface KatvanTextFinderClient ()
@property (nonatomic) katvan::Editor* editor;
@end

@implementation KatvanTextFinderClient

- (instancetype)initWithEditor:(katvan::Editor*)editor
{
    self = [super init];
    if (self) {
        self.editor = editor;
    }
    return self;
}

- (NSUInteger)stringLength
{
    return self.editor->document()->characterCount();
}

- (NSString*)stringAtIndex:(NSUInteger)characterIndex
             effectiveRange:(NSRangePointer)outRange
             endsWithSearchBoundary:(BOOL*)outFlag
{
    QTextBlock block = self.editor->document()->findBlock(characterIndex);
    if (!block.isValid()) {
        return nil;
    }

    // QTextBlock::text() strips the block separator
    QString text = block.text() + QChar::LineFeed;

    *outRange = NSMakeRange(block.position(), block.length());
    *outFlag = YES; // Each block is a search boundary
    return text.toNSString();
}

- (BOOL)isEditable
{
    return !self.editor->isReadOnly();
}

- (BOOL)allowsMultipleSelection
{
    return NO;
}

- (NSRange)firstSelectedRange
{
    QTextCursor cursor = self.editor->textCursor();
    if (!cursor.hasSelection()) {
        return NSMakeRange(cursor.position(), 0);
    }
    return NSMakeRange(cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart());
}

- (QTextCursor)textCursorForRange:(NSRange)range
{
    QTextCursor cursor { self.editor->document() };
    cursor.setPosition(range.location);
    cursor.setPosition(range.location + range.length, QTextCursor::KeepAnchor);
    return cursor;
}

- (NSArray<NSValue*>*)selectedRanges
{
    return @[
        [NSValue valueWithRange: [self firstSelectedRange]]
    ];
}

- (void)setSelectedRanges:(NSArray<NSValue*>*)selectedRanges
{
    if (selectedRanges.count == 0) {
        return;
    }

    NSRange range = [selectedRanges[0] rangeValue];
    self.editor->goToBlock([self textCursorForRange:range], false);
}

- (void)scrollRangeToVisible:(NSRange)range
{
    self.editor->showPosition(range.location);
}

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString*)string
{
    QTextCursor cursor = [self textCursorForRange:range];
    cursor.insertText(QString::fromNSString(string));
}

@end
