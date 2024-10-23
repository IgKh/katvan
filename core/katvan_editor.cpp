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
#include "katvan_codemodel.h"
#include "katvan_editor.h"
#include "katvan_editorlayout.h"
#include "katvan_highlighter.h"
#include "katvan_spellchecker.h"
#include "katvan_text_utils.h"

#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QShortcut>
#include <QTextBlock>
#include <QTimer>
#include <QToolTip>

namespace katvan {

static constexpr QKeyCombination TEXT_DIRECTION_TOGGLE(Qt::CTRL | Qt::SHIFT | Qt::Key_X);
static constexpr QKeyCombination INSERT_POPUP(Qt::CTRL | Qt::SHIFT | Qt::Key_I);

static constexpr int MAX_LINE_FOR_MODELINES = 10;

Q_GLOBAL_STATIC(QRegularExpression, MODELINE_REGEX, QStringLiteral("((kate|katvan):.+)$"))

Q_GLOBAL_STATIC(QSet<QString>, OPENING_BRACKETS, {
    QStringLiteral("("), QStringLiteral("{"),  QStringLiteral("["),
    QStringLiteral("$"), QStringLiteral("\""), QStringLiteral("<"),
    QStringLiteral("*"), QStringLiteral("_")
})

Q_GLOBAL_STATIC(QSet<QString>, CLOSING_BRACKETS, {
    QStringLiteral(")"), QStringLiteral("}"),  QStringLiteral("]"),
    QStringLiteral("$"), QStringLiteral("\""), QStringLiteral(">"),
    QStringLiteral("*"), QStringLiteral("_")
})

Q_GLOBAL_STATIC(QSet<QString>, DEDENTING_CLOSING_BRACKETS, {
    QStringLiteral(")"), QStringLiteral("}"), QStringLiteral("]")
})

class LineNumberGutter : public QWidget
{
public:
    LineNumberGutter(Editor *editor) : QWidget(editor), d_editor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(d_editor->lineNumberGutterWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        d_editor->lineNumberGutterPaintEvent(this, event);
    }

private:
    Editor *d_editor;
};

Editor::Editor(QWidget* parent)
    : QTextEdit(parent)
    , d_theme(EditorTheme::defaultTheme())
    , d_suppressContentChangeHandling(false)
    , d_pendingSuggestionsPosition(-1)
{
    document()->setDocumentLayout(new EditorLayout(document()));
    setAcceptRichText(false);

    connect(SpellChecker::instance(), &SpellChecker::suggestionsReady, this, &Editor::spellingSuggestionsReady);
    connect(SpellChecker::instance(), &SpellChecker::dictionaryChanged, this, &Editor::forceRehighlighting);

    d_highlighter = new Highlighter(document(), SpellChecker::instance(), d_theme);
    d_codeModel = new CodeModel(document(), this);

    d_leftLineNumberGutter = new LineNumberGutter(this);
    d_rightLineNumberGutter = new LineNumberGutter(this);

    connect(document(), &QTextDocument::contentsChange, this, &Editor::propagateDocumentEdit);
    connect(document(), &QTextDocument::blockCountChanged, this, &Editor::updateLineNumberGutterWidth);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &Editor::updateLineNumberGutters);
    connect(this, &QTextEdit::textChanged, this, &Editor::updateLineNumberGutters);
    connect(this, &QTextEdit::cursorPositionChanged, this, &Editor::updateLineNumberGutters);
    connect(this, &QTextEdit::cursorPositionChanged, this, &Editor::updateExtraSelections);

    updateLineNumberGutters();

    QShortcut* toggleDirection = new QShortcut(this);
    toggleDirection->setKey(TEXT_DIRECTION_TOGGLE);
    toggleDirection->setContext(Qt::WidgetShortcut);
    connect(toggleDirection, &QShortcut::activated, this, &Editor::toggleTextBlockDirection);

    QShortcut* insertPopup = new QShortcut(this);
    insertPopup->setKey(INSERT_POPUP);
    insertPopup->setContext(Qt::WidgetShortcut);
    connect(insertPopup, &QShortcut::activated, this, &Editor::popupInsertMenu);

    d_debounceTimer = new QTimer(this);
    d_debounceTimer->setSingleShot(true);
    d_debounceTimer->setInterval(500);
    d_debounceTimer->callOnTimeout(this, &Editor::contentModified);

    QTimer::singleShot(0, this, &Editor::updateEditorTheme);
}

void Editor::applySettings(const EditorSettings& settings)
{
    d_appSettings = settings;
    applyEffectiveSettings();
}

void Editor::applyEffectiveSettings()
{
    EditorSettings origSettings = d_effectiveSettings;

    d_effectiveSettings = EditorSettings();
    d_effectiveSettings.mergeSettings(d_appSettings);
    d_effectiveSettings.mergeSettings(d_fileMode);

    if (origSettings == d_effectiveSettings) {
        return;
    }

    setFont(d_effectiveSettings.font());

    QTextOption textOption = document()->defaultTextOption();
    textOption.setTabStopDistance(d_effectiveSettings.tabWidth() * fontMetrics().horizontalAdvance(QLatin1Char(' ')));

    QTextOption::Flags flags;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (d_effectiveSettings.showControlChars()) {
        flags |= QTextOption::ShowDefaultIgnorables;
    }
#endif

    textOption.setFlags(flags);
    document()->setDefaultTextOption(textOption);

    EditorSettings::LineNumberStyle lineNumberStyle = d_effectiveSettings.lineNumberStyle();
    if (layoutDirection() == Qt::LeftToRight) {
        d_leftLineNumberGutter->setVisible(lineNumberStyle != EditorSettings::LineNumberStyle::NONE);
        d_rightLineNumberGutter->setVisible(lineNumberStyle == EditorSettings::LineNumberStyle::BOTH_SIDES);
    }
    else {
        d_rightLineNumberGutter->setVisible(lineNumberStyle != EditorSettings::LineNumberStyle::NONE);
        d_leftLineNumberGutter->setVisible(lineNumberStyle == EditorSettings::LineNumberStyle::BOTH_SIDES);
    }
    updateLineNumberGutters();
}

void Editor::updateEditorTheme()
{
    EditorTheme& newTheme = EditorTheme::defaultTheme();
    if (d_theme.name() == newTheme.name()) {
        return;
    }

    d_theme = newTheme;
    setPalette(d_theme.adjustPalette(window()->palette()));
    forceRehighlighting();
    updateExtraSelections();
}

QMenu* Editor::createInsertMenu()
{
    QMenu* menu = new QMenu();

    menu->addAction(tr("Right-to-Left Mark"), this, [this]() { insertMark(utils::RLM_MARK); });
    menu->addAction(tr("Left-to-Right Mark"), this, [this]() { insertMark(utils::LRM_MARK); });
    menu->addAction(tr("Arabic Letter Mark"), this, [this]() { insertMark(utils::ALM_MARK); });

    menu->addSeparator();

    menu->addAction(tr("Right-to-Left Isolate"), this, [this]() { insertSurroundingMarks(utils::RLI_MARK, utils::PDI_MARK); });
    menu->addAction(tr("Left-to-Right Isolate"), this, [this]() { insertSurroundingMarks(utils::LRI_MARK, utils::PDI_MARK); });

    menu->addSeparator();

    QAction* insertInlineMathAction = menu->addAction(tr("Inline &Math"), this, [this]() {
        insertSurroundingMarks(utils::LRI_MARK + QStringLiteral("$"), QStringLiteral("$") + utils::PDI_MARK);
    });
    insertInlineMathAction->setShortcut(Qt::CTRL | Qt::Key_M);

    return menu;
}

QString Editor::documentTextForPreview() const
{
    return document()->toPlainText() + QChar::LineFeed;
}

void Editor::setDocumentText(const QString& text)
{
    QScopedValueRollback guard { d_suppressContentChangeHandling, true };
    document()->setPlainText(text);
}

void Editor::toggleTextBlockDirection()
{
    QTextBlock currentBlock = textCursor().block();
    Qt::LayoutDirection currentDirection = currentBlock.layout()->textOption().textDirection();
    if (currentDirection == Qt::LeftToRight) {
        setTextBlockDirection(Qt::RightToLeft);
    }
    else {
        setTextBlockDirection(Qt::LeftToRight);
    }
}

void Editor::setTextBlockDirection(Qt::LayoutDirection dir)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::StartOfBlock);

    QString blockText = cursor.block().text();

    if (dir == Qt::RightToLeft) {
        if (blockText.startsWith(utils::LRM_MARK)) {
            cursor.deleteChar();
            blockText = blockText.sliced(1);
        }
        if (!blockText.startsWith(utils::RLM_MARK)
            && !blockText.startsWith(utils::ALM_MARK)
            && utils::naturalTextDirection(blockText) != dir) {
            cursor.insertText(utils::RLM_MARK);
        }
    }
    else if (dir == Qt::LeftToRight) {
        if (blockText.startsWith(utils::RLM_MARK) || blockText.startsWith(utils::ALM_MARK)) {
            cursor.deleteChar();
            blockText = blockText.sliced(1);
        }
        if (!blockText.startsWith(utils::LRM_MARK) && utils::naturalTextDirection(blockText) != dir) {
            cursor.insertText(utils::LRM_MARK);
        }
    }

    cursor.endEditBlock();
}

void Editor::goToBlock(int blockNum, int charOffset)
{
    QTextBlock block = document()->findBlockByNumber(blockNum);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor { block };
    if (charOffset > 0) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, charOffset);
    }
    setTextCursor(cursor);
    setFocus();
}

void Editor::forceRehighlighting()
{
    QTimer::singleShot(0, d_highlighter, &QSyntaxHighlighter::rehighlight);
}

void Editor::checkForModelines()
{
    EditorSettings mode;

    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() < MAX_LINE_FOR_MODELINES) {
        QRegularExpressionMatch match = MODELINE_REGEX->match(block.text());
        if (match.hasMatch()) {
            EditorSettings lineMode(match.captured());
            mode.mergeSettings(lineMode);
        }
        block = block.next();
    }

    d_fileMode = mode;
    applyEffectiveSettings();
}

void Editor::showToolTip(QPoint widgetPos, const QString& text)
{
    if (!d_pendingTooltipPos || d_pendingTooltipPos.value() != widgetPos) {
        return;
    }
    d_pendingTooltipPos.reset();

    QToolTip::showText(mapToGlobal(widgetPos), text, this);
}

void Editor::handleToolTipEvent(QHelpEvent* event)
{
    QPoint documentPoint = event->pos()
        - viewport()->geometry().topLeft()
        + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

    int offset = document()->documentLayout()->hitTest(documentPoint, Qt::ExactHit);
    if (offset >= 0) {
        QTextCursor cursor(document());
        cursor.setPosition(offset);

        d_pendingTooltipPos = event->pos();
        Q_EMIT toolTipRequested(cursor.blockNumber(), cursor.positionInBlock(), event->pos());
    }
    else {
        QToolTip::hideText();
        event->ignore();
    }
}

bool Editor::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        handleToolTipEvent(helpEvent);
    }
#if defined(Q_OS_LINUX)
    else if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) ||
            keyEvent->keyCombination() == QKeyCombination(Qt::ControlModifier, Qt::Key_Shift)) {
            if (keyEvent->nativeScanCode() == 50) {
                d_pendingDirectionChange = Qt::LeftToRight;
            }
            else if (keyEvent->nativeScanCode() == 62) {
                d_pendingDirectionChange = Qt::RightToLeft;
            }
            else {
                d_pendingDirectionChange.reset();
            }
        }
    }
#endif
    return QTextEdit::event(event);
}

void Editor::contextMenuEvent(QContextMenuEvent* event)
{
    QTextCursor cursor = cursorForPosition(event->pos());
    QString misspelledWord = misspelledWordAtCursor(cursor);

    d_contextMenu = createStandardContextMenu(event->pos());
    d_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

    if (!misspelledWord.isEmpty()) {
        QAction* origFirstAction = d_contextMenu->actions().first();

        QAction* placeholderAction = new QAction(tr("Calculating Suggestions..."));
        placeholderAction->setEnabled(false);

        QAction* addToPersonalAction = new QAction(tr("Add to Personal Dictionary"));
        connect(addToPersonalAction, &QAction::triggered, this, [this, misspelledWord, cursor]() {
            SpellChecker::instance()->addToPersonalDictionary(misspelledWord);
            forceRehighlighting();
        });

        d_contextMenu->insertAction(origFirstAction, placeholderAction);
        d_contextMenu->insertAction(origFirstAction, addToPersonalAction);
        d_contextMenu->insertSeparator(origFirstAction);
    }

    d_contextMenu->addSeparator();
    d_contextMenu->addAction(tr("Toggle Text Direction"), TEXT_DIRECTION_TOGGLE, this, &Editor::toggleTextBlockDirection);

    if (!misspelledWord.isEmpty()) {
        // Request the suggestions after menu was created, but before it is
        // shown. If suggestions are already in cache, the suggestionsReady
        // signal will be instantly invoked as a direct connection.
        d_pendingSuggestionsWord = misspelledWord;
        d_pendingSuggestionsPosition = cursor.position();
        SpellChecker::instance()->requestSuggestions(misspelledWord, cursor.position());
    }
    d_contextMenu->popup(event->globalPos());
}

std::tuple<QTextBlock, QTextBlock, bool> Editor::selectedBlockRange() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return std::make_tuple(cursor.block(), cursor.block(), false);
    }

    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    QTextBlock startBlock = document()->findBlock(selectionStart);
    QTextBlock endBlock = document()->findBlock(selectionEnd);

    bool fullFirstBlock = selectionStart == startBlock.position() &&
                          selectionEnd >= (endBlock.position() + endBlock.length() - 1);

    return std::make_tuple(startBlock, endBlock, fullFirstBlock);
}

QString Editor::getIndentString(QTextCursor cursor) const
{
    if (d_effectiveSettings.indentStyle() == EditorSettings::IndentStyle::SPACES) {
        qsizetype numSpaces = d_effectiveSettings.indentWidth() - (cursor.positionInBlock() % d_effectiveSettings.indentWidth());
        return QString(numSpaces, QLatin1Char(' '));
    }
    return QLatin1String("\t");
}

static bool isSingleBidiMark(QChar ch)
{
    return ch == utils::LRM_MARK || ch == utils::RLM_MARK || ch == utils::ALM_MARK;
}

static bool isSpace(QChar ch)
{
    return ch.category() == QChar::Separator_Space || ch == QLatin1Char('\t') || isSingleBidiMark(ch);
}

static bool isAllWhitespace(const QString& text)
{
    return std::all_of(text.begin(), text.end(), isSpace);
}

static QString getLeadingIndent(const QString& text)
{
    qsizetype i = 0;
    while (i < text.size() && isSpace(text[i])) {
        i++;
    }
    return text.left(i).removeIf(isSingleBidiMark);
}

static void cursorSkipWhite(QTextCursor& cursor)
{
    QString blockText = cursor.block().text();
    while (!cursor.atBlockEnd()) {
        if (isSpace(blockText[cursor.positionInBlock()])) {
            cursor.movePosition(QTextCursor::NextCharacter);
        }
        else {
            break;
        }
    }
}

static bool cursorInLeadingWhitespace(const QTextCursor& cursor)
{
    QString blockText = cursor.block().text();
    QString textBeforeCursor = blockText.sliced(0, cursor.positionInBlock());
    return isAllWhitespace(textBeforeCursor);
}

void Editor::handleNewLine()
{
    QTextCursor cursor = textCursor();
    QString initialIndent = getLeadingIndent(cursor.block().text());

    int origPos = cursor.position();
    QTextBlock origBlock = cursor.block();

    cursor.beginEditBlock();
    cursor.insertBlock();

    if (d_effectiveSettings.indentMode() != EditorSettings::IndentMode::NONE) {
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.insertText(initialIndent);

        if (d_effectiveSettings.indentMode() == EditorSettings::IndentMode::SMART) {
            d_highlighter->reparseBlock(origBlock);

            if (d_codeModel->shouldIncreaseIndent(origPos)) {
                cursorSkipWhite(cursor);
                QTextBlock indentBlock = d_codeModel->findMatchingIndentBlock(cursor.position());

                cursor.insertText(getIndentString(cursor));

                // If return is pressed directly between two delimiters, move
                // the closing bracket to another new line, with no indent
                // relative to original. Cursor should be at end of first new
                // block.
                if (indentBlock == origBlock) {
                    int savedPos = cursor.position();
                    cursor.insertBlock();
                    cursor.insertText(initialIndent);
                    cursor.setPosition(savedPos);
                }
            }
        }
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
}

void Editor::handleClosingBracket(const QString& bracket)
{
    QTextCursor cursor = textCursor();
    int origPos = cursor.position();
    bool wasAtLeadingWhitespace = cursorInLeadingWhitespace(cursor);

    cursor.beginEditBlock();
    cursor.insertText(bracket);

    // Auto dedent
    if (wasAtLeadingWhitespace && d_effectiveSettings.indentMode() == EditorSettings::IndentMode::SMART)
    {
        d_highlighter->reparseBlock(cursor.block());

        QTextBlock indentBlock = d_codeModel->findMatchingIndentBlock(origPos);
        if (indentBlock != cursor.block())
        {
            // Select everything from start of block, and replace with correct indent
            cursor.movePosition(QTextCursor::PreviousCharacter);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            cursor.insertText(getLeadingIndent(indentBlock.text()));
            cursor.movePosition(QTextCursor::NextCharacter);
        }
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void Editor::unindentBlock(QTextCursor blockStartCursor, QTextCursor notAfter)
{
    QTextCursor cursor = blockStartCursor;
    while (document()->characterAt(cursor.position()).category() == QChar::Other_Format) {
        cursor.movePosition(QTextCursor::MoveOperation::NextCharacter);
    }

    if (!notAfter.isNull() && cursor >= notAfter) {
        return;
    }

    // Remove up to one tab or indentWidth spaces from begining of block
    cursor.beginEditBlock();
    if (d_effectiveSettings.indentStyle() == EditorSettings::IndentStyle::TABS) {
        if (document()->characterAt(cursor.position()) == QLatin1Char('\t')) {
            cursor.deleteChar();
        }
    }
    else {
        int numSpacesToDelete = d_effectiveSettings.indentWidth();
        if (!notAfter.isNull()) {
            numSpacesToDelete = std::min(numSpacesToDelete, std::max(0, notAfter.position() - cursor.position()));
        }

        while (document()->characterAt(cursor.position()).category() == QChar::Separator_Space
               && numSpacesToDelete > 0) {
            cursor.deleteChar();
            numSpacesToDelete--;
        }
    }
    cursor.endEditBlock();
}

void Editor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return) {
        handleNewLine();
        return;
    }

    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab || event->key() == Qt::Key_Backspace) {
        auto [selectionBlockStart, selectionBlockEnd, fullFirstBlock] = selectedBlockRange();

        if (event->keyCombination() == QKeyCombination(Qt::Key_Tab)) {
            if (fullFirstBlock || selectionBlockEnd.position() > selectionBlockStart.position()) {
                // Indent all blocks in selection
                QTextCursor cursor(selectionBlockStart);
                cursor.beginEditBlock();

                for (auto it = selectionBlockStart; it.isValid(); it = it.next()) {
                    if (selectionBlockEnd < it) {
                        break;
                    }

                    cursorSkipWhite(cursor);
                    if (!cursor.atBlockEnd()) {
                        cursor.insertText(getIndentString(cursor));
                    }
                    cursor.movePosition(QTextCursor::NextBlock);
                }
                cursor.endEditBlock();
            }
            else {
                // Insert one indent at cursor
                QTextCursor cursor = textCursor();
                cursor.insertText(getIndentString(cursor));
            }
            return;
        }
        else if (event->key() == Qt::Key_Backtab) {
            // Unindent all blocks in range
            QTextCursor cursor(selectionBlockStart);
            cursor.beginEditBlock();

            for (auto it = selectionBlockStart; it.isValid(); it = it.next()) {
                if (selectionBlockEnd < it) {
                    break;
                }
                unindentBlock(cursor);
                cursor.movePosition(QTextCursor::NextBlock);
            }
            cursor.endEditBlock();
            return;
        }
        else if (event->keyCombination() == QKeyCombination(Qt::Key_Backspace)) {
            // Unindent current block only, if cursor is in leading whitespace
            QTextCursor cursor = textCursor();
            if (!cursor.hasSelection() && !cursor.atBlockStart()) {
                if (cursorInLeadingWhitespace(cursor)) {
                    unindentBlock(QTextCursor(cursor.block()), cursor);
                    return;
                }
            }
        }
    }

    if (event->matches(QKeySequence::MoveToStartOfLine) || event->matches(QKeySequence::SelectStartOfLine)) {
        QTextCursor cursor = textCursor();
        if (cursor.atBlockStart() || !cursorInLeadingWhitespace(cursor)) {
            QTextCursor c { cursor.block() };
            cursorSkipWhite(c);

            if (event->matches(QKeySequence::SelectStartOfLine)) {
                cursor.setPosition(c.position(), QTextCursor::KeepAnchor);
                setTextCursor(cursor);
            }
            else {
                setTextCursor(c);
            }
            return;
        }
    }

    if (CLOSING_BRACKETS->contains(event->text())) {
        QTextCursor cursor = textCursor();
        if (!cursor.atBlockEnd() && cursor.block().text().at(cursor.positionInBlock()) == event->text().at(0)) {
            cursor.movePosition(QTextCursor::NextCharacter);
            setTextCursor(cursor);
            return;
        }
        else if (DEDENTING_CLOSING_BRACKETS->contains(event->text())) {
            handleClosingBracket(event->text());
            return;
        }
    }
    if (OPENING_BRACKETS->contains(event->text())) {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection() || cursor.atBlockEnd() || !cursor.block().text().at(cursor.positionInBlock()).isLetterOrNumber()) {
            auto closingBracket = d_codeModel->getMatchingCloseBracket(textCursor(), event->text().at(0));
            if (closingBracket) {
                insertSurroundingMarks(event->text(), QString(closingBracket.value()));
                return;
            }
        }
    }

#if defined(Q_OS_WINDOWS)
    if (event->key() == Qt::Key_Direction_L) {
        setTextBlockDirection(Qt::LeftToRight);
        return;
    }
    if (event->key() == Qt::Key_Direction_R) {
        setTextBlockDirection(Qt::RightToLeft);
        return;
    }
#elif defined(Q_OS_MACOS)
    if (event->keyCombination().keyboardModifiers() == (Qt::MetaModifier | Qt::ShiftModifier)) {
        if (event->nativeVirtualKey() == 0x38) { // kVK_Shift
            d_pendingDirectionChange = Qt::LeftToRight;
            return;
        }
        else if (event->nativeVirtualKey() == 0x3C) { // kVK_RightShift
            d_pendingDirectionChange = Qt::RightToLeft;
            return;
        }
        else {
            d_pendingDirectionChange.reset();
        }
    }
#endif

    QTextEdit::keyPressEvent(event);
}

void Editor::keyReleaseEvent(QKeyEvent* event)
{
    if (d_pendingDirectionChange) {
        setTextBlockDirection(d_pendingDirectionChange.value());
        d_pendingDirectionChange.reset();
        return;
    }
    QTextEdit::keyReleaseEvent(event);
}

void Editor::resizeEvent(QResizeEvent* event)
{
    QTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    int gutterWidth = lineNumberGutterWidth();
    int verticalScrollBarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;

    if (d_leftLineNumberGutter->isVisible()) {
        if (layoutDirection() == Qt::LeftToRight) {
            d_leftLineNumberGutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth, cr.height()));
        }
        else {
            d_leftLineNumberGutter->setGeometry(QRect(cr.left() + verticalScrollBarWidth, cr.top(), gutterWidth, cr.height()));
        }
    }
    if (d_rightLineNumberGutter->isVisible()) {
        if (layoutDirection() == Qt::LeftToRight) {
            d_rightLineNumberGutter->setGeometry(QRect(cr.right() - gutterWidth - verticalScrollBarWidth, cr.top(), gutterWidth, cr.height()));
        }
        else {
            d_rightLineNumberGutter->setGeometry(QRect(cr.right() - gutterWidth, cr.top(), gutterWidth, cr.height()));
        }
    }
}

void Editor::propagateDocumentEdit(int from, int charsRemoved, int charsAdded)
{
    if (d_suppressContentChangeHandling) {
        return;
    }

    QTextBlock block = document()->findBlock(from);

    // In Qt's internal reckoning all blocks always end with ParagraphSeparator
    // or other such marker. Global character positions (like the from parameter
    // here) take this into account, however QTextBlock::text() strips them away.
    // To make sure that our and the Typst compiler's character positions are
    // always in sync, we explcitly add a line feed in each place where a
    // separator character would be in the document's internal buffer.
    QString blockText = block.text().sliced(from - block.position()) + QChar::LineFeed;

    QString editText;
    while (block.isValid() && charsAdded > 0) {
        if (blockText.size() > charsAdded) {
            editText += blockText.left(charsAdded);
            break;
        }
        else {
            editText += blockText;
            charsAdded -= blockText.size();
            block = block.next();
            blockText = block.text() + QChar::LineFeed;
        }
    }

    Q_EMIT contentEdited(from, from + charsRemoved, editText);
    d_debounceTimer->start();
}

void Editor::popupInsertMenu()
{
    QMenu* insertMenu = createInsertMenu();
    insertMenu->setAttribute(Qt::WA_DeleteOnClose);

    QPoint globalPos = viewport()->mapToGlobal(cursorRect().topLeft());
    insertMenu->exec(globalPos);
}

std::tuple<int, int> Editor::misspelledRangeAtCursor(QTextCursor cursor)
{
    if (cursor.isNull()) {
        return std::make_tuple(-1, 0);
    }

    size_t pos = cursor.positionInBlock();
    QTextBlock block = cursor.block();

    HighlighterStateBlockData* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
    if (!blockData) {
        return std::make_tuple(-1, 0);
    }

    const auto& words = blockData->misspelledWords();
    for (const auto& w : words) {
        if (pos >= w.startPos && pos <= w.startPos + w.length) {
            return std::make_tuple(block.position() + static_cast<int>(w.startPos), static_cast<int>(w.length));
        }
    }
    return std::make_tuple(-1, 0);
}

QString Editor::misspelledWordAtCursor(QTextCursor cursor)
{
    auto [pos, length] = misspelledRangeAtCursor(cursor);
    if (pos < 0) {
        return QString();
    }

    QTextBlock block = cursor.block();
    return block.text().sliced(pos - block.position(), length);
}

void Editor::spellingSuggestionsReady(const QString& word, int position, const QStringList& suggestions)
{
    if (!d_contextMenu) {
        return;
    }

    if (d_pendingSuggestionsWord != word || d_pendingSuggestionsPosition != position) {
        return;
    }
    d_pendingSuggestionsWord.clear();
    d_pendingSuggestionsPosition = -1;

    QAction* suggestionsPlaceholder = d_contextMenu->actions().first();
    if (suggestions.isEmpty()) {
        suggestionsPlaceholder->setText(tr("No Suggestions Available"));
    }
    else {
        QMenu* suggestionsMenu = new QMenu(tr("%n Suggestion(s)", "", suggestions.size()));
        for (const QString& suggestion : suggestions) {
            suggestionsMenu->addAction(QString(suggestion), this, [this, position, suggestion]() {
                changeMisspelledWordAtPosition(position, suggestion);
            });
        }

        d_contextMenu->insertMenu(suggestionsPlaceholder, suggestionsMenu);
        d_contextMenu->removeAction(suggestionsPlaceholder);
    }
}

void Editor::changeMisspelledWordAtPosition(int position, const QString& into)
{
    QTextCursor cursor{ document() };
    cursor.setPosition(position);

    auto [startPos, length] = misspelledRangeAtCursor(cursor);
    if (startPos < 0) {
        return;
    }

    cursor.setPosition(startPos);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, length);
    cursor.insertText(into);
}

void Editor::insertMark(QChar mark)
{
    textCursor().insertText(mark);
}

void Editor::insertSurroundingMarks(QString before, QString after)
{
    QTextCursor cursor = textCursor();
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();

    cursor.insertText(before + cursor.selectedText() + after);
    cursor.setPosition(selectionStart + before.length(), QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectionEnd - selectionStart);
    setTextCursor(cursor);
}

int Editor::lineNumberGutterWidth()
{
    int digits = 1;
    int max = qMax(1, document()->blockCount());
    while (max >= 10) {
        max /= 10;
        digits++;
    }

    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void Editor::updateLineNumberGutterWidth()
{
    int gutterWidth = lineNumberGutterWidth();

    int leftMargin = 0;
    if (d_leftLineNumberGutter->isVisible()) {
        leftMargin = gutterWidth + 1;
    }

    int rightMargin = 0;
    if (d_rightLineNumberGutter->isVisible()) {
        rightMargin = gutterWidth + 1;
    }

    setViewportMargins(leftMargin, 0, rightMargin, 0);
}

void Editor::updateLineNumberGutters()
{
    QRect cr = contentsRect();
    d_leftLineNumberGutter->update(0, cr.y(), d_leftLineNumberGutter->width(), cr.height());
    d_rightLineNumberGutter->update(0, cr.y(), d_rightLineNumberGutter->width(), cr.height());

    updateLineNumberGutterWidth();

    int dy = verticalScrollBar()->sliderPosition();
    if (dy >= 0) {
        d_leftLineNumberGutter->scroll(0, dy);
        d_rightLineNumberGutter->scroll(0, dy);
    }
}

QTextEdit::ExtraSelection Editor::makeBracketHighlight(int pos)
{
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(d_theme.editorColor(EditorTheme::EditorColor::MATCHING_BRACKET));
    selection.cursor = QTextCursor(document());
    selection.cursor.setPosition(pos);
    selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    return selection;
}

void Editor::updateExtraSelections()
{
    QTextCursor cursor = textCursor();
    int currentPos = cursor.position();

    cursor.clearSelection();

    QList<QTextEdit::ExtraSelection> extraSelections;

    //
    // Current Line
    //
    QTextEdit::ExtraSelection currentLine;
    currentLine.format.setBackground(d_theme.editorColor(EditorTheme::EditorColor::CURRENT_LINE));
    currentLine.format.setProperty(QTextFormat::FullWidthSelection, true);
    currentLine.cursor = cursor;
    extraSelections.append(currentLine);

    //
    // Bracket Matching
    //
    auto bracketPos = d_codeModel->findMatchingBracket(currentPos);
    if (bracketPos) {
        extraSelections.append(makeBracketHighlight(currentPos));
        extraSelections.append(makeBracketHighlight(bracketPos.value()));
    }
    else if (!cursor.atBlockStart()) {
        bracketPos = d_codeModel->findMatchingBracket(currentPos - 1);
        if (bracketPos) {
            extraSelections.append(makeBracketHighlight(currentPos - 1));
            extraSelections.append(makeBracketHighlight(bracketPos.value()));
        }
    }

    setExtraSelections(extraSelections);
}

void Editor::lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event)
{
    QColor fgColor = d_theme.editorColor(EditorTheme::EditorColor::FOREGROUND);
    QColor editorBgColor = d_theme.editorColor(EditorTheme::EditorColor::BACKGROUND);
    QColor gutterBgColor = d_theme.editorColor(EditorTheme::EditorColor::GUTTER);
    QBrush secondaryBgBrush(gutterBgColor, Qt::Dense6Pattern);

    QPainter painter(gutter);
    painter.setClipRect(event->rect());

    if (d_effectiveSettings.lineNumberStyle() == EditorSettings::LineNumberStyle::BOTH_SIDES) {
        painter.fillRect(event->rect(), editorBgColor);
        painter.fillRect(event->rect(), secondaryBgBrush);
    }
    else {
        painter.fillRect(event->rect(), gutterBgColor);
    }

    // The visible part of the document, in document coordinates
    QRect viewportGeometry = viewport()->geometry();
    QRect documentVisibleRect = viewport()->geometry()
        .translated(-viewportGeometry.topLeft())
        .translated(horizontalScrollBar()->value(), verticalScrollBar()->value());

    QTextDocument* doc = document();
    int blockNumberUnderCursor = textCursor().blockNumber();

    EditorLayout* layout = qobject_cast<EditorLayout*>(doc->documentLayout());
    QTextBlock block = layout->findContainingBlock(documentVisibleRect.top());

    qreal additionalMargin = 0;
    if (block.blockNumber() == 0) {
        additionalMargin = doc->documentMargin() - 1 - verticalScrollBar()->sliderPosition();
    }

    QRectF blockRect = layout->blockBoundingRect(block);
    qreal top = viewportGeometry.top() + additionalMargin;
    qreal bottom = top + blockRect.intersected(documentVisibleRect).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        Qt::LayoutDirection blockDirection = block.layout()->textOption().textDirection();

        QBrush bgBrush;
        int textFlags;
        int textOffset;
        if (gutter == d_leftLineNumberGutter) {
            bgBrush = (blockDirection == Qt::RightToLeft) ? secondaryBgBrush : gutterBgColor;
            textFlags = (layoutDirection() == Qt::RightToLeft) ? Qt::AlignLeft : Qt::AlignRight;
            textOffset = -5;
        }
        else {
            bgBrush = (blockDirection == Qt::RightToLeft) ? gutterBgColor : secondaryBgBrush;
            textFlags = (layoutDirection() == Qt::RightToLeft) ? Qt::AlignRight : Qt::AlignLeft;
            textOffset = 5;
        }

        if (d_effectiveSettings.lineNumberStyle() == EditorSettings::LineNumberStyle::BOTH_SIDES) {
            QRect bgRect(0, top, gutter->width(), bottom - top);
            painter.fillRect(bgRect, bgBrush);
        }

        // Draw the line number only if the block's top is visible
        if (blockRect.top() >= documentVisibleRect.top()) {
            QString number = QString::number(block.blockNumber() + 1);

            QFont f = gutter->font();
            if (block.blockNumber() == blockNumberUnderCursor) {
                f.setWeight(QFont::ExtraBold);
            }
            painter.setFont(f);
            painter.setPen(fgColor);

            QRectF textRect(textOffset, top, gutter->width(), painter.fontMetrics().height());
            painter.drawText(textRect, textFlags, number);
        }

        block = block.next();
        blockRect = layout->blockBoundingRect(block);
        top = bottom;
        bottom = top + blockRect.intersected(documentVisibleRect).height();
    }
}

}
