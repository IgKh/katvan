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
#import "macshell_widgets.h"

#include <QtMinMax>

@interface KatvanAuxToolBar ()

@property (nonatomic) NSStackView* stackView;

@end

@implementation KatvanAuxToolBar

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        self.stackView = [NSStackView stackViewWithViews:@[]];
        self.stackView.edgeInsets = NSEdgeInsetsMake(4, 8, 4, 8);
        self.stackView.spacing = 4;
        [self addSubview:self.stackView];

        NSBox* separator = [[NSBox alloc] init];
        separator.boxType = NSBoxSeparator;
        separator.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:separator];

        [NSLayoutConstraint activateConstraints:@[
            [self.stackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.stackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
            [self.stackView.heightAnchor constraintEqualToConstant:25.0],
            [separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [separator.topAnchor constraintEqualToAnchor:self.stackView.bottomAnchor],
            [separator.bottomAnchor constraintEqualToAnchor:self.bottomAnchor]
        ]];
    }
    return self;
}

- (void)addView:(NSView*)view inGravity:(NSStackViewGravity)gravity
{
    [self.stackView addView:view inGravity:gravity];
}

- (NSButton*)addButtonWithIcon:(NSImage*)icon
        toolTip:(NSString*)toolTip
        inGravity:(NSStackViewGravity)gravity
        target:(id)target
        action:(SEL)selector
{
    NSButton* button = [NSButton buttonWithImage:icon target:target action:selector];
    button.toolTip = toolTip;
    button.bezelStyle = NSBezelStyleToolbar;
    button.bordered = NO;
    button.showsBorderOnlyWhileMouseInside = YES;
    button.symbolConfiguration = [NSImageSymbolConfiguration configurationWithScale:NSImageSymbolScaleLarge];

    [self.stackView addView:button inGravity:gravity];
    return button;
}

@end

@interface KatvanSpinBox ()

@property (nonatomic) NSTextField* textField;
@property (nonatomic) NSStepper* stepper;

@end

@implementation KatvanSpinBox

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        _minimum = 0;
        _maximum = 99;
        _value = 0;

        self.textField = [NSTextField textFieldWithString:@""];
        self.textField.translatesAutoresizingMaskIntoConstraints = NO;
        self.textField.delegate = self;
        self.textField.alignment = NSTextAlignmentRight;
        self.textField.font = [NSFont monospacedDigitSystemFontOfSize:NSFont.systemFontSize weight:NSFontWeightRegular];
        self.textField.stringValue = [self formattedStringForValue:_value];

        self.stepper = [[NSStepper alloc] initWithFrame:NSZeroRect];
        self.stepper.translatesAutoresizingMaskIntoConstraints = NO;
        self.stepper.minValue = _minimum;
        self.stepper.maxValue = _maximum;
        self.stepper.increment = 1;
        self.stepper.integerValue = _value;
        self.stepper.valueWraps = NO;
        self.stepper.autorepeat = YES;
        self.stepper.target = self;
        self.stepper.action = @selector(stepperChanged:);

        [self addSubview:self.textField];
        [self addSubview:self.stepper];

        [NSLayoutConstraint activateConstraints:@[
            [self.textField.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.textField.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.stepper.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.stepper.leadingAnchor constraintEqualToAnchor:self.textField.trailingAnchor constant:4.0],
            [self.stepper.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.heightAnchor constraintGreaterThanOrEqualToConstant:22.0],
        ]];

        [self setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];
    }
    return self;
}

- (NSSize)intrinsicContentSize
{
    NSSize textFieldSize = self.textField.intrinsicContentSize;

    return NSMakeSize(
        textFieldSize.width + 4.0 + self.stepper.intrinsicContentSize.width,
        textFieldSize.height
    );
}

- (void)setMinimum:(NSInteger)minimum
{
    _minimum = minimum;
    self.stepper.minValue = minimum;
    self.value = _value;
}

- (void)setMaximum:(NSInteger)maximum
{
    _maximum = maximum;
    self.stepper.maxValue = (double)maximum;
    self.value = _value;
}

- (void)setValue:(NSInteger)value
{
    NSInteger clamped = qBound(_minimum, value, _maximum);
    _value = clamped;
    _stepper.integerValue = clamped;
    [self updateTextField];
}

- (void)setEnabled:(BOOL)enabled
{
    _enabled = enabled;
    self.textField.enabled = enabled;
    self.stepper.enabled = enabled;
}

- (void)sendAction
{
    if (_action) {
        [NSApp sendAction:_action to:_target from:self];
    }
}

- (void)updateTextField
{
    self.textField.stringValue = [self formattedStringForValue:_value];
}

- (NSString*)formattedStringForValue:(NSInteger)value
{
    return [NSString stringWithFormat:@"%ld", static_cast<long>(value)];
}

- (NSInteger)parseTextField
{
    NSString* raw = _textField.stringValue;

    raw = [raw stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSScanner* scanner = [NSScanner scannerWithString:raw];

    NSInteger result = 0;
    BOOL ok = [scanner scanInteger:&result];
    if (ok && scanner.isAtEnd) {
        return result;
    }
    return NSNotFound;
}

- (void)stepperChanged:(id)sender
{
    NSInteger newValue = self.stepper.integerValue;
    if (newValue == _value) {
        return;
    }

    _value = newValue;
    [self updateTextField];
    [self sendAction];
}

- (void)commitTextFieldValue
{
    NSInteger parsed = [self parseTextField];
    if (parsed == NSNotFound) {
        // Invalid input, revert to the last good value
        [self updateTextField];
        return;
    }

    NSInteger clamped = qBound(_minimum, parsed, _maximum);
    if (clamped != _value) {
        _value = clamped;
        _stepper.integerValue = _value;
        [self sendAction];
    }
    _textField.stringValue = [self formattedStringForValue:_value];
}

- (void)controlTextDidEndEditing:(NSNotification*)obj
{
    [self commitTextFieldValue];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    if (commandSelector == @selector(insertNewline:)) {
        [self commitTextFieldValue];
        return YES;
    }
    return NO;
}

@end

@interface KatvanFontPicker ()

@property (nonatomic) NSButton* selectorButton;
@property (nonatomic) NSTextField* fontLabel;

@end

@implementation KatvanFontPicker

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        _font = [NSFont userFontOfSize:[NSFont systemFontSize]];

        self.selectorButton = [NSButton buttonWithTitle:NSLocalizedString(@"Change…", "Button to select another font")
                                        target:self
                                        action:@selector(selectFont:)];

        self.selectorButton.translatesAutoresizingMaskIntoConstraints = NO;

        self.fontLabel = [NSTextField labelWithString:@""];
        self.fontLabel.translatesAutoresizingMaskIntoConstraints = NO;

        [self updateFontLabel];

        [self addSubview:self.selectorButton];
        [self addSubview:self.fontLabel];

        [NSLayoutConstraint activateConstraints:@[
            [self.selectorButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.selectorButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.fontLabel.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.fontLabel.leadingAnchor constraintEqualToAnchor:self.selectorButton.trailingAnchor constant:12.0],
            [self.fontLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.heightAnchor constraintGreaterThanOrEqualToConstant:22.0],
        ]];

        [self.selectorButton setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
        [self setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];
    }
    return self;
}

- (NSSize)intrinsicContentSize
{
    NSSize selectorButtonSize = self.selectorButton.intrinsicContentSize;
    NSSize fontLabelSize = self.fontLabel.intrinsicContentSize;

    return NSMakeSize(
        selectorButtonSize.width + 12.0 + fontLabelSize.width,
        qMax(selectorButtonSize.height, fontLabelSize.height)
    );
}

- (BOOL)acceptsFirstResponder
{
    // Must be first responder for the NSFontChanging stuff to work correctly
    return YES;
}

- (BOOL)resignFirstResponder
{
    [[NSFontPanel sharedFontPanel] close];
    return YES;
}

- (void)setFont:(NSFont*)font
{
    _font = font;
    [self updateFontLabel];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];

    if (self.window) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(parentWindowWillClose:)
                                              name:NSWindowWillCloseNotification
                                              object:self.window];
    }
    else {
        [[NSNotificationCenter defaultCenter] removeObserver:self];
    }
}

- (void)parentWindowWillClose:(NSNotification*)notification
{
    if (self.window.firstResponder == self) {
        [self.window makeFirstResponder:nil];
    }
}

- (void)updateFontLabel
{
    self.fontLabel.stringValue = [NSString stringWithFormat:@"%@ %ld", _font.displayName, static_cast<long>(_font.pointSize)];
    [self invalidateIntrinsicContentSize];
}

- (void)selectFont:(id)sender
{
    NSFontManager* manager = [NSFontManager sharedFontManager];
    NSFontPanel* panel = [NSFontPanel sharedFontPanel];

    [manager setSelectedFont:_font isMultiple:NO];

    [self.window makeFirstResponder:self];
    [panel makeKeyAndOrderFront:self.window];
}

- (void)changeFont:(NSFontManager*)fontManager
{
    NSFont* font = [fontManager convertFont:_font];
    if ([font isEqual:_font]) {
        return;
    }

    _font = font;
    [self updateFontLabel];

    if (_action) {
        [NSApp sendAction:_action to:_target from:self];
    }
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel*)fontPanel
{
    return NSFontPanelModesMaskStandardModes & ~NSFontPanelModeMaskAllEffects;
}

@end
