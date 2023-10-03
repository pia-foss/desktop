// Copyright (c) 2023 Private Internet Access, Inc.
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

#include <common/src/common.h>
#line HEADER_FILE("balancetext.h")

#ifndef BALANCETEXT_H
#define BALANCETEXT_H

namespace BalanceText {

// Wrap text to a width that provides a balanced appearance in line widths.
// Mainly used by InfoTip.
//
// This returns a width to use to render the text with a balanced appearance.
// This is approximately the width that minimizes whitespace within the text
// bound, though it may be larger to extend the first line if it improves the
// appearance of the right edges of the lines.
//
// This assumes that the text is rendered with the `Text.Wrap` wrap mode.
//
// The text can contain hard line breaks.  If the hard line breaks prevent the
// text from wrapping, then this pass has no effect (it effectively just
// measures the text).  If the text still wraps, the hard line breaks are
// preserved and the width is minimized to optimize appearance.  The underhang
// reduction pass is not (currently) applied to text with hard line breaks.
//
// Parameters:
// - maxWidth - The maximum width allowed for this text - the return value
//   will not be (much) greater than this value.  (Rounding and a
//   Windows-specific fudge factor could cause it to be slightly greater.)
// - fontPixelSize - The pixel size of the font (the default application
//   font is used)
// - text - The text to be measured and wrapped
//
// Returns the calculated text width.  The value calculated could be up to
// ~2 px larger than the true minimum, but for display purposes this is
// fine.  (The bisection search used has a 1-px convergence threshold, and
// we round the result up to the next whole pixel.)
//
// This computation typically takes around 20-50 ms (roughly measured on a
// Win 10 VM).  This is fast enough for InfoTip to do the computation just
// before it's shown, but slow enough that it shouldn't be recomputed and
// stored all the time for InfoTips that aren't visible.
//
// Note that the measured result could vary by platform, as different platforms
// have different default hinting modes (the text appearance really does vary
// slightly by platform; it's unusual for it to be enough to affect wrapping
// though.)  Measurements are performed using a generic "screen" paint context
// (the default for QTextLayout); this should be independent of scaling factors
// by virtue of the way PIA windows perform scaling.  (It's not possible to get
// a QPaintDevice from a QQuickWindow; the QtQuick scene graph rendering does
// not use QPaintDevice.)
//
// Thai text varies significantly by platform; Qt is unable to properly
// word-break Thai text on Windows/Mac (probably due to being built without
// ICU).  On Windows/Mac, it will wrap using spaces in the text, if there are
// any.  On Linux, Qt can properly word-break the text.
double balanceWrappedText(double maxWidth, int fontPixelSize,
                          const QString &text);

}

#endif
