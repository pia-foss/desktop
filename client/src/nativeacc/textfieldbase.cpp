// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line SOURCE_FILE("textfieldbase.cpp")

#include "textfieldbase.h"
#include "accutil.h"

namespace NativeAcc {

TextFieldBase::TextFieldBase(QAccessible::Role role, QQuickItem &item)
    : AccessibleItem{role, item}, _cursorPos{0}
{
}

QString TextFieldBase::maskedValueMid(const QString &value, int pos, int n) const
{
    if(isPasswordEdit())
    {
        int maxCount = value.length() - pos;
        if(maxCount < 0)
            return {};  // Pos is at or past the end of the string
        if(n < 0 || n > maxCount)
            n = maxCount;
        // Return a string of the appropriate length, but replaced with bullets.
        // (This is what the UI displays, so that's what the screen reader
        // should read.)
        auto result = QString{n, u'\u2022'};   // Bullet character
        qInfo() << "masked mid:" << result;
        return result;
    }

    return value.mid(pos, n);
}

void TextFieldBase::setValue(const QString &value)
{
    // Nothing to do if the value is unchanged.
    // However, this also prevents us from emitting a text change if the user
    // pastes/types identical text replacing a selection, that should be fine
    // since the text didn't change.
    if(_value == value || !item())
        return;

    QString oldValue = std::move(_value);   // Needed for change event
    _value = std::move(value);
    emit valueChanged();

    // Don't emit value changes for password edits.
    //
    // On Mac, while typing, this would cause VoiceOver to read the bullet
    // character that was added as "1 space c h a..." - it's spelling out
    // "1 character" since it doesn't know how to read the bullet, but it's
    // supposed to be spelling.
    //
    // It still doesn't play the "key strike" sound that it normally plays for
    // password edits, but it's not clear what causes it to do that.
    if(isPasswordEdit())
        return;

    // The value change is supposed to indicate the position of the change, the
    // part of the text that was replaced (if any), and the new text at that
    // position (if any).
    //
    // The implementation would be obvious if we were actually handling all the
    // interactions (key press, paste, etc.), but we're not.  We basically have
    // to diff the two values.  Since we can only report one change range
    // anyway, just find the common prefix and suffix strings.
    //
    // This will be slightly inaccurate if the user replaces a selection (by
    // typing or pasting), and part of the the new text matches the beginning or
    // end of the old text.  The TextInput QML type does not give us the info
    // needed to do it right, though.
    //
    // For example, if the old state was "my [text] value", with the bracketed
    // text selected, and "new text" is pasted, we'll say "replaced '' with
    // 'new '" instead of "replaced 'text' with 'new text'".
    //
    // There's also ambiguity if duplicate text is inserted, such as typing a
    // 't' in 'seatle' to get 'seattle', we can't tell which 't' was inserted.
    // It might be possible to refine this by checking the cursor position.
    int prefixLen = 0;
    while(prefixLen < _value.count() && prefixLen < oldValue.count() &&
        _value[prefixLen] == oldValue[prefixLen])
    {
        ++prefixLen;
    }

    int suffixLen = 0;
    // Don't include any characters that we considered part of the common prefix
    // in the common suffix.  (suffixLen < _value.count() - prefixLen, etc.)
    // This is for ambiguous cases like "seat" -> "seatt", the final 't' is put
    // in the prefix by default, which is most often correct.
    while(suffixLen < _value.count()-prefixLen &&
          suffixLen < oldValue.count()-prefixLen &&
          _value[_value.count()-suffixLen-1] == oldValue[oldValue.count()-suffixLen-1])
    {
        ++suffixLen;
    }

    // There necessarily has to be a difference, since the values are checked to
    // make sure they're actually different.
    // The sum could equal one of the string sizes (for a pure insertion/
    // deletion), but it can't be greater.
    Q_ASSERT(prefixLen + suffixLen <= _value.count());
    Q_ASSERT(prefixLen + suffixLen <= oldValue.count());

    // Regular mid() is fine (not maskedValueMid()) since we checked for a
    // password edit above.
    QString deleted = oldValue.mid(prefixLen, oldValue.count() - prefixLen - suffixLen);
    QString inserted = _value.mid(prefixLen, _value.count() - prefixLen - suffixLen);

    // The strings can't both be empty since we know the text changed.
    Q_ASSERT(!inserted.isEmpty() || !deleted.isEmpty());

    if(deleted.isEmpty())
    {
        QAccessibleTextInsertEvent textChange{item(), prefixLen, inserted};
        QAccessible::updateAccessibility(&textChange);
    }
    else if(inserted.isEmpty())
    {
        QAccessibleTextRemoveEvent textChange{item(), prefixLen, deleted};
        QAccessible::updateAccessibility(&textChange);
    }
    else
    {
        QAccessibleTextUpdateEvent textChange{item(), prefixLen, deleted, inserted};
        QAccessible::updateAccessibility(&textChange);
    }
}

void TextFieldBase::setCursorPos(int cursorPos)
{
    if(cursorPos == _cursorPos || !item())
        return;

    _cursorPos = cursorPos;
    emit cursorPosChanged();

    QAccessibleTextCursorEvent cursorChange{item(), _cursorPos};
    QAccessible::updateAccessibility(&cursorChange);
}

QString TextFieldBase::textValue() const
{
    return maskedValueMid(_value, 0);
}

void TextFieldBase::addSelection(int, int)
{
}

QString TextFieldBase::attributes(int offset, int *startOffset, int *endOffset) const
{
    // No support for attributes
    if(startOffset)
        *startOffset = offset;
    if(endOffset)
        *endOffset = offset;
    return {};
}

int TextFieldBase::characterCount() const
{
    return _value.size();
}

QRect TextFieldBase::characterRect(int offset) const
{
    // TODO - might have to implement this, would depend on the exact control
    // that is attached
    // Could implement this with TextField.positionToRectangle()
    return item() ? itemScreenRect(*item()) : QRect{};
}

int TextFieldBase::cursorPosition() const
{
    return _cursorPos;
}

int TextFieldBase::offsetAtPoint(const QPoint &point) const
{
    return 0;   // Not implemented
}

void TextFieldBase::removeSelection(int)
{
}

void TextFieldBase::scrollToSubstring(int, int)
{
}

void TextFieldBase::selection(int, int *startOffset, int *endOffset) const
{
    if(startOffset)
        *startOffset = 0;
    if(endOffset)
        *endOffset = 0;
}

int TextFieldBase::selectionCount() const
{
    return 0;
}

void TextFieldBase::setCursorPosition(int position)
{
    emit changeCursorPos(position);
}

void TextFieldBase::setSelection(int, int, int)
{
}

QString TextFieldBase::text(int startOffset, int endOffset) const
{
    if(endOffset <= startOffset)
        return {};
    return maskedValueMid(_value, startOffset, endOffset-startOffset);
}

}
