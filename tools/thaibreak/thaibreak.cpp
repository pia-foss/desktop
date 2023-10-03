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

#include <thai/thwchar.h>
#include <thai/thwbrk.h>
#include <iostream>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <memory>
#include <cstring>
#include <regex>
#include <cassert>

void showHelp(const char *argv0)
{
    std::cout << "usage:" << std::endl;
    std::cout << "  " << argv0 << " [--verbose] [--regex <expr>]" << std::endl;
    std::cout << "  " << argv0 << " --help" << std::endl;
    std::cout << std::endl;
    std::cout << "Inserts zero-width spaces into Thai text read from standard input." << std::endl;
    std::cout << "The resulting text is written to standard output." << std::endl;
    std::cout << "Input and output are both UTF-8." << std::endl;
    std::cout << std::endl;
    std::cout << "  --verbose: Print character counts to standard error as input is processed" << std::endl;
    std::cout << "  --regex: Transform parts of the input matching a regex; group 1 will be" << std::endl;
    std::cout << "    filtered as Thai text.  Non-matching lines are passed unmodified." << std::endl;
    std::cout << "  --help: Show this help" << std::endl;
}

// Convert between UTF-8 and UTF-32.  libthai only supports TIS-620 for 8-bit
// characters.
// On Linux, wchar_t is 32 bits, so this results in UTF-32 (technically
// UCS-4 but the difference is unimportant here).  If wchar_t was
// 16 bits instead, this would convert to UCS-2, which does
// meaningfully differ from UTF-16 (although only if characters outside
// of the BMP are used, which is unlikely for PIA).
std::wstring fromUtf8(const std::string &utf8)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.from_bytes(utf8);
}
std::string toUtf8(const std::wstring &utf32)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.to_bytes(utf32);
}

class LibthaiDeleter
{
public:
    void operator()(ThBrk *p) const {if(p) ::th_brk_delete(p);}
};

const std::wstring zeroWidthSpaceDelim{L"\x200b"};

template<class T>
using LibthaiPtr = std::unique_ptr<T, LibthaiDeleter>;

bool isThai(wchar_t c)
{
    // Thai is in the U+0Exx block.  (Technically U+0E01 - U+0E5B are assigned,
    // but we can safely assume that any of these codepoints are Thai since this
    // is just used to skip word delimiters around non-Thai characters.)
    return (c & 0xFFFFFF00) == 0x00000E00;
}

std::wstring insertSpaces(ThBrk &breaker, const std::wstring &inputUtf32)
{
    // We use libthai to determine the break positions, but we only need to
    // insert zero width spaces when a break position is between two Thai
    // characters.
    //
    // Libthai will tell us all possible break positions, including around
    // spaces and punctuation.  Qt is still able to break those on its own at
    // runtime, and in particular inserting a break between "%1" would break
    // argument substitution.

    // Worst-case, there could be a break between every character, so the
    // upper bound on the number of returned break positions is the length of
    // the string.
    std::vector<int> breakPositions;
    breakPositions.resize(inputUtf32.size());

    int positionsFound = ::th_brk_wc_find_breaks(&breaker, inputUtf32.c_str(),
                                                 &breakPositions[0],
                                                 breakPositions.size());
    breakPositions.resize(positionsFound);

    // Copy to a new string, inserting breaks as appropriate
    std::wstring outputUtf32;
    outputUtf32.reserve(inputUtf32.size() + zeroWidthSpaceDelim.size() * positionsFound);

    // Copy the leader before the first break position
    auto itInputPos = inputUtf32.begin();
    auto itBreakPos = breakPositions.begin();
    while(itBreakPos != breakPositions.end())
    {
        // Copy the part of the string up to this break position, then check if
        // we need a delimiter
        auto itRangeBegin = inputUtf32.begin() + *itBreakPos;
        std::swap(itInputPos, itRangeBegin);
        outputUtf32.insert(outputUtf32.end(), itRangeBegin, itInputPos);

        // Insert the break delimiter if we're in between two valid chars, and
        // both of those are Thai codepoints
        if(itInputPos != inputUtf32.begin() && itInputPos != inputUtf32.end() &&
            isThai(*itInputPos) && isThai(*(itInputPos-1)))
        {
            outputUtf32.insert(outputUtf32.end(), zeroWidthSpaceDelim.begin(),
                               zeroWidthSpaceDelim.end());
        }

        ++itBreakPos;
    }

    // Copy the tail after the last break position
    outputUtf32.insert(outputUtf32.end(), itInputPos, inputUtf32.end());

    return outputUtf32;
}

std::string thaiBreak(ThBrk &breaker, const std::string &input, bool verbose)
{
    std::size_t rawUtf8Length = input.size();
    std::wstring inputUtf32{fromUtf8(input)};
    std::size_t rawUtf32Length = inputUtf32.size();

    // Remove any zero width spaces that might already exist
    auto cleanedEnd = std::remove(inputUtf32.begin(), inputUtf32.end(),
                                  zeroWidthSpaceDelim[0]);
    inputUtf32.erase(cleanedEnd, inputUtf32.end());
    std::size_t cleanedUtf32Length = inputUtf32.size();

    // Insert zero width spaces at word breaks
    std::wstring outputUtf32 = insertSpaces(breaker, inputUtf32);

    // Convert back to UTF-8
    auto outputUtf8 = toUtf8(outputUtf32);

    // Print diagnostics if verbose tracing is enabled
    if(verbose)
    {
        std::cerr << "Found "
            << (outputUtf32.size() - cleanedUtf32Length)
            << " breaks.  UTF-8: " << rawUtf8Length << ", UTF-32: "
            << rawUtf32Length << " (cleaned: " << cleanedUtf32Length
            << ") ==> UTF-32: " << outputUtf32.size() << ", UTF-8: "
            << outputUtf8.size() << std::endl;
    }

    return outputUtf8;
}

int main(int argc, char **argv)
{
    if(argc < 1 || !argv[0])
        return -1;

    bool verbose = false;

    std::regex matchRegex{"(.*)"};

    for(int i=1; i<argc; ++i)
    {
        if(std::strcmp(argv[i], "--help") == 0)
        {
            showHelp(argv[0]);
            return 0;
        }
        else if(std::strcmp(argv[i], "--verbose") == 0)
        {
            verbose = true;
        }
        else if(std::strcmp(argv[i], "--regex") == 0)
        {
            ++i;
            if(i >= argc)
            {
                std::cerr << "Option --regex requires argument" << std::endl;
                return -1;
            }

            matchRegex = std::regex{argv[i]};
            if(matchRegex.mark_count() < 1)
            {
                std::cerr << "Argument to --regex must have a capture group indicating the Thai text to process" << std::endl;
                return -1;
            }
        }
        else
        {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            return -1;
        }
    }

    LibthaiPtr<ThBrk> pThaiBrk{::th_brk_new(nullptr)};
    if(!pThaiBrk)
    {
        std::cerr << "libthai failed to load Thai dictionary" << std::endl;
        std::cerr << "Check if LIBTHAI_DICTDIR environment variable needs to be set" << std::endl;
        return -1;
    }

    std::string input;
    std::smatch lineMatch;
    while(std::getline(std::cin, input))
    {
        if(std::regex_match(input, lineMatch, matchRegex))
        {
            // Consequence of regex_match() and the fact that we checked
            // mark_count() for a user-supplied regex
            assert(lineMatch.size() >= 2);

            // Extract the prefix, match range, and suffix.  If the regex has a
            // capture group but it somehow did not participate in the match
            // (like if it is an alternate not taken), then lineMatch[1] is
            // {end(), end()} and this passes through normally with no
            // substitutions taking place.
            std::string prefix{input.cbegin(), lineMatch[1].first};
            std::string match{lineMatch[1].first, lineMatch[1].second};
            std::string suffix{lineMatch[1].second, input.cend()};
            if(verbose)
            {
                std::cerr << "Matched range " << prefix.size() << "-"
                    << (prefix.size() + match.size()) << " of " << input.size()
                    << "-character input" << std::endl;
            }
            std::cout << prefix << thaiBreak(*pThaiBrk, match, verbose)
                << suffix << std::endl;
        }
        else
        {
            // Doesn't match the regex, just pass as-is
            std::cout << input << std::endl;
        }
    }

    return 0;
}
