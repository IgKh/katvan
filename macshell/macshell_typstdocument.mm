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
#import "macshell_typstdocument.h"
#import "macshell_windowcontroller.h"

#include "katvan_document.h"

@interface KatvanTypstDocument ()

@property katvan::Document* textDocument;

@end

@implementation KatvanTypstDocument {
    BOOL d_recoveredFromBackup;
    BOOL d_modifiedSinceAutosave;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        d_recoveredFromBackup = NO;
        d_modifiedSinceAutosave = NO;

        self.textDocument = new katvan::Document();

        // We don't use AppKit's undo manager - Qt has its own undo system, but we
        // do need to ensure that the document is notified about undo operations.
        self.hasUndoManager = NO;

        __weak __typeof__(self) weakSelf = self;
        QObject::connect(self.textDocument, &QTextDocument::modificationChanged,
                        self.textDocument, [weakSelf]() {
            [weakSelf syncModificationStatus];
        });
        QObject::connect(self.textDocument, &katvan::Document::contentModified,
                        self.textDocument, [weakSelf]() {
            [weakSelf contentWasModified];
        });
    }
    return self;
}

- (void)dealloc
{
    delete self.textDocument;
}

- (void)makeWindowControllers
{
    KatvanWindowController* windowController = [[KatvanWindowController alloc] initWithDocument:self.textDocument initialURL:self.fileURL];
    [self addWindowController:windowController];
}

+ (BOOL)autosavesInPlace
{
    return YES;
}

- (BOOL)isDocumentEdited
{
    return d_recoveredFromBackup || self.textDocument->isModified();
}

- (BOOL)hasUnautosavedChanges
{
    return d_modifiedSinceAutosave;
}

- (void)updateChangeCount:(NSDocumentChangeType)change
{
    BOOL wasModified = [self isDocumentEdited];

    if (change == NSChangeCleared) {
        d_recoveredFromBackup = NO;
        d_modifiedSinceAutosave = NO;
        self.textDocument->setModified(false);
    }
    else if (change == NSChangeReadOtherContents) {
        d_recoveredFromBackup = YES;
    }
    else if (change == NSChangeAutosaved) {
        d_modifiedSinceAutosave = NO;
    }

    BOOL isModified = [self isDocumentEdited];
    if (isModified != wasModified) {
        [self syncModificationStatus];
    }
}

- (void)updateChangeCountWithToken:(id)changeCountToken forSaveOperation:(NSSaveOperationType)saveOperation
{
    if (saveOperation == NSSaveOperation || saveOperation == NSSaveAsOperation) {
        [self updateChangeCount:NSChangeCleared];
    }
    else if (saveOperation == NSAutosaveInPlaceOperation || saveOperation == NSAutosaveElsewhereOperation) {
        [self updateChangeCount:NSChangeAutosaved];
    }
}

- (void)contentWasModified
{
    d_modifiedSinceAutosave = YES;
    [self scheduleAutosaving];
}

- (void)syncModificationStatus
{
    // Ensure that the NSDocument internal edited state is in sync with our own
    // modification tracking logic. It does all kinds of magic with it, so the
    // way to ensure that we don't skip that, is to ensure that the original
    // updateChangeCount: method is called only here, and only by us.
    BOOL edited = [self isDocumentEdited];
    if (edited) {
        [super updateChangeCount:NSChangeDone];
    }
    else {
        [super updateChangeCount:NSChangeCleared];
    }
}

- (BOOL)readFromData:(NSData*)data ofType:(NSString*)typeName error:(NSError**)outError
{
    if (![typeName isEqualToString:@"app.typst.document"]) {
        return NO;
    }

    QByteArray bytes = QByteArray::fromRawNSData(data);
    self.textDocument->setPlainText(QString::fromUtf8(bytes));
    self.textDocument->setModified(false);

    return YES;
}

- (BOOL) writeToURL:(NSURL*)url
         ofType:(NSString*)typeName
         forSaveOperation:(NSSaveOperationType)saveOperation
         originalContentsURL:(NSURL*)absoluteOriginalContentsURL
         error:(NSError**)outError
{
    QByteArray bytes = self.textDocument->toPlainText().toUtf8();

    NSData* data = bytes.toRawNSData();
    if (![data writeToURL:url options:0 error:outError]) {
        return NO;
    }

    if (saveOperation == NSSaveOperation || saveOperation == NSSaveAsOperation) {
        // Explicit save
        KatvanWindowController* controller = self.windowControllers[0];
        [controller documentDidExplicitlySaveInURL:self.fileURL forced:NO];
    }
    return YES;
}

@end
