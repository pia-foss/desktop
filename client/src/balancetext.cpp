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
#line SOURCE_FILE("balancetext.cpp")

#include "balancetext.h"
#include <QTextLayout>
#include <QGuiApplication>
#include <cmath>

namespace BalanceText {

// Compute the underhang cost for a line pair.  Compares the lines' lengths and
// applies either the underhang or overhang factor as appropriate.
//
// Returns a nonnegative cost value.
double computeLinePairUnderhangCost(const QTextLine &priorLine, const QTextLine &currentLine,
                                    double underhangFactor, double overhangFactor)
{
    // Compute the length difference.  Positive is an overhang, negative is
    // an underhang
    double lengthDiff = priorLine.naturalTextWidth() - currentLine.naturalTextWidth();

    // Apply cost factors depending on whether this is an over- or underhang
    if(lengthDiff > 0) // Overhang
        return lengthDiff * overhangFactor;
    else    // Underhang
        return -lengthDiff * underhangFactor;
}

// Compute an underhang cost score based on how the lines break at the given
// width.  Overhangs are generally preferred to underhangs, especially for the
// first pair of lines.
//
// This lays out the QTextLayout again (destroying any existing layout).  It
// does not set positions for any of the text lines that are created.
double scoreUnderhangCost(double width, QTextLayout &layout)
{
    // An underhang looks much uglier than an overhang; these cost more.
    const double underhangFactor = 1.75;
    const double overhangFactor = 1.0;

    // An underhang on the first line pair looks _especially_ unnatural, give it
    // an additional cost factor.  (The normal underhang factor is also
    // applied.)
    const double firstUnderhangFactor = 1.5;

    layout.beginLayout();

    QTextLine firstLine;
    QTextLine currentLine;

    firstLine = layout.createLine();
    if(!firstLine.isValid())
    {
        layout.endLayout();
        return 0;   // No lines at all
    }
    firstLine.setLineWidth(width);

    // The first valid line pair gets the extra firstUnderhangFactor, after that
    // it's just the regular underhangFactor
    double lineUnderhangFactor = firstUnderhangFactor * underhangFactor;
    double totalCost = 0;
    while(true)
    {
        QTextLine nextLine = layout.createLine();
        if(!nextLine.isValid())
            break;
        nextLine.setLineWidth(width);

        // Compare all lines to the first line.  This produces the best result
        // for texts with 3 or more lines - the alignment of the right edge with
        // the first line is more eye-catching than the differences between each
        // successive pair of lines.
        totalCost += computeLinePairUnderhangCost(firstLine, nextLine,
                                                  lineUnderhangFactor,
                                                  overhangFactor);
        lineUnderhangFactor = underhangFactor;  // No longer on first line
    }

    layout.endLayout();

    return totalCost;
}

// Measure text wrapped to a given width.
// This is similar to QFontMetricsF::boudingRect(const QRect&, int, const QString&),
// but it returns the actual width of the text as well as the measured height.
// (boundingRect() always returns the given width).
//
// This is necessary if the text cannot be wrapped to the given width (the
// minimization algorithm can't necessarily detect this otherwise because the
// line count does not change.)
QSizeF measureWrappedText(double width, QTextLayout &layout)
{
    QSizeF size;

    int nextLineBreak = layout.text().indexOf('\n');

    layout.beginLayout();
    while(true)
    {
        QTextLine line = layout.createLine();
        if(!line.isValid())
            break;
        line.setLineWidth(width);
        // QTextLine doesn't break on line breaks by default.  Stop at a line
        // break if it passed one.
        if(nextLineBreak >= 0 && line.textStart() + line.textLength() > nextLineBreak)
        {
            // Instead fill up to (and including) the next line break
            line.setNumColumns(nextLineBreak - line.textStart() + 1);
            // Find the next line break (if there is one)
            nextLineBreak = layout.text().indexOf('\n', nextLineBreak+1);
        }

        double lineWidth = line.naturalTextWidth();
        if(size.width() < lineWidth)
            size.setWidth(lineWidth);
        size.rheight() += line.leading() + line.height();
    }

    layout.endLayout();

    return size;
}

double reduceUnderhang(double minWidth, double maxWidth, QTextLayout &layout)
{
    // This algorithm doesn't work if the text contains hard line breaks.
    // QTextLayout doesn't handle them; we'd have to write code to manually
    // split up those lines.  Currently, there are no texts with hard line
    // breaks that benefit from this pass anyway.
    if(layout.text().indexOf('\n') >= 0)
        return minWidth;

    double minWidthCost = scoreUnderhangCost(minWidth, layout);

    // Figure out the width of the first line if we added one more word to it.
    // We can find the next break position by breaking the first line, then
    // creating a second line with a width of 1 px, since QTextLine consumes
    // text up to the first possible break position at a minimum.
    //
    // In principle, we could find a break position like this for each line
    // other than the last, but these other positions would necessarily create
    // an underhang, so they're unlikely to be chosen.
    //
    // The whole reduceUnderhang() pass usually adds roughly 2-5 ms right now;
    // this would add a lot of time for virtually no benefit.
    layout.beginLayout();
    QTextLine firstLine = layout.createLine();
    if(!firstLine.isValid())
    {
        layout.endLayout();
        return minWidth;    // No lines, nothing to do
    }
    firstLine.setLineWidth(minWidth);
    QTextLine secondLine = layout.createLine();
    if(!secondLine.isValid())
    {
        layout.endLayout();
        return minWidth;    // Only one line, nothing to do
    }
    secondLine.setLineWidth(1);
    layout.endLayout();

    int firstLineNextBreakPos = secondLine.textStart() + secondLine.textLength();

    // Measure the width of the first line with one additional word
    double infiniteWidth = 65536.0;
    QTextLayout firstLineLayout{layout.text().left(firstLineNextBreakPos), layout.font()};
    QSizeF widerSize = measureWrappedText(infiniteWidth, firstLineLayout);
    if(widerSize.width() > maxWidth)
        return minWidth;    // Won't fit at all, don't bother with the second layout

    // Score the wider width and see if its overhang is preferable
    double widerWidthCost = scoreUnderhangCost(widerSize.width(), layout);

    if(widerWidthCost < minWidthCost)
        return widerSize.width();
    return minWidth;
}

double balanceWrappedText(double maxWidth, int fontPixelSize,
                          const QString &text)
{
    QFont font = QGuiApplication::font();
    font.setPixelSize(fontPixelSize);
    QTextLayout layout{text, font};

    // Get the minimum text height (at the maximum width)
    QSizeF minHeightSize = measureWrappedText(maxWidth, layout);
    double minHeight = minHeightSize.height();

    // Use a bisection search to find the smallest width that doesn't increase
    // the height
    double widthLowBound = 0;
    double widthHighBound = maxWidth;
    while(widthHighBound - widthLowBound > 1.0)
    {
        double widthGuess = (widthHighBound + widthLowBound) * 0.5;

        // Measure at the guess
        QSizeF measuredSize = measureWrappedText(widthGuess, layout);

        // It's possible for the returned bound to be wider than the width
        // specified if the text simply cannot be broken down to that width
        // (there is a single word that's too wide).  This happens a lot in
        // Thai (check the location/country no-PF tips).  Theoretically it could
        // happen for a single-word text in English.
        if(measuredSize.width() > widthGuess)
        {
            // The guess is definitely too small, but don't just raise the
            // lower bound to the guess, raise it all the way to the minimum
            // width, any widths smaller than this are impossible.
            //
            // Note that the height in this case might still be the minimum
            // height, so the height test to raise the lower bound might not
            // otherwise catch this.
            widthLowBound = measuredSize.width();
        }
        else if(measuredSize.height() > minHeight)
        {
            // The width guess is too small, increase the low bound
            widthLowBound = widthGuess;
        }
        else
        {
            // The width guess is still large enough to hold the text, reduce
            // the high bound.
            //
            // Note that the measured size might be lower, and it would seem
            // safe to reduce the high bound to that size, potentially saving
            // some iterations.
            //
            // However, it does not always hold that we can lay out at the
            // measured width and still get the same layout.  This seems to be
            // related to hinting (?).
            //
            // For example, on Linux (hinting is platform-dependent), the en_US
            // info tip for the low-MTU setting measures to 387.531 px, but even
            // laying out at 388.0 causes it to break to an extra line.  389
            // seems to be the minimum width that satisfies Qt.
            //
            // This happens the most on Windows (it's actually rare on Linux);
            // Thai text especially tends to have large error.  (All other
            // languages seem to need ~1px fudge, but Thai seems to need ~3px.)
            // It never happens on Mac, probably because Cocoa does not support
            // font hinting at all.
            //
            // So, just reduce to the measured width; since we don't know how
            // much "wiggle room" Qt needs to preserve this layout.  (If Qt
            // could just return us the "minimum layout bound" for this layout,
            // it would have saved a lot of headaches.)
            widthHighBound = widthGuess;
        }
    }

    // Use the high bound as the balanced width.  We have a convergence
    // tolerance of 1px, so the low bound might be 1 px too small for the text.
    //
    // It's also possible for the lower bound to be greater than the higher
    // bound here if the maximum width wasn't possible - the text contains a
    // word longer than the maximum width.  Using the high bound is correct in
    // this case too, because it's maxWidth and we should not return a value
    // significantly larger than that.
    double balancedWidth = widthHighBound;

    // Check for underhang reduction.  May increase the width if it improves the
    // appearance of the text, but won't increase it beyond maxWidth.
    balancedWidth = reduceUnderhang(balancedWidth, maxWidth, layout);

    // Round up to an integer.  On Windows at least, QQuickText seems to lose
    // the fractional part of the number before actually performing the layout,
    // causing extra breaks.  This shouldn't be enough change to affect the
    // layout.
    balancedWidth = std::ceil(balancedWidth);

#ifdef Q_OS_WIN
    // Ugh, we still need a slight fudge factor on Windows.  The measurement
    // process is all fine up to this point, but when we put the text/width on a
    // QQuickText, it can come out a little off in a few cases.
    // - The Swedish port forwarding info tip measures 434.5 but wraps to an
    //   extra line even at 435.0 px in QQuickText.
    // - The Thai MACE info tip requires 1 px fudge
    // - The Thai port forwarding info tip is the worst, it requires 2 px of
    //   fudge (here the overhang compensation covers for it, but disabling that
    //   shows 2 px error for the minimized layout).
    //
    // This doesn't seem to happen on any other platform (even with hinting on
    // Linux/FreeType, it seems).
    balancedWidth += 2.0;
#endif

    return balancedWidth;
}

}
