// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("semversion.cpp")

#include <limits>
#include "semversion.h"

namespace
{
    // For some reason, QStringView does not provide indexOf().
    qsizetype indexOf(const QStringView &str, QChar c, qsizetype from=0)
    {
        while(from < str.size())
        {
            if(str[from] == c)
                return from;
            ++from;
        }
        return -1;
    }
}

int PrereleaseTag::fieldToInt(QStringView field)
{
    // If it's a numeric tag, store the numeric value and leave _stringValue
    // empty.  Note that QString's various integer functions allow whitespace,
    // etc., so we check the value manually.  QString's functions also do not
    // appear to check for overflow.
    // We also do not use QChar::isDigit(), QChar::digitValue(), etc., because
    // those include digits other than 0-9 from other scripts.

    if(field.isEmpty())
        return NonNumeric;

    // Check to see if the field is numeric first.  (It could be non-numeric but
    // contain enough leading digits to trigger 'overflow' if we tried to parse
    // at the same time.)
    // Semver does not allow numeric values with leading zeroes for either
    // major/minor/patch or prerelease tags, but these are tolerated here.
    for(const QChar c : field)
    {
        if(c < '0' || c > '9')
            return NonNumeric;  // Not a numeric field
    }

    // Parse it.
    int value = 0;
    for(const QChar c : field)
    {
        if(value > std::numeric_limits<int>::max() / 10)
            throw Error{HERE, Error::Code::VersionUnparseableError};
        value *= 10;
        int digit = c.unicode() - '0';
        if(value > std::numeric_limits<int>::max() - digit)
            throw Error{HERE, Error::Code::VersionUnparseableError};
        value += digit;
    }

    return value;
}

PrereleaseTag::PrereleaseTag(QStringView tag)
    : _numericValue{}
{
    // Empty prerelease tags are not allowed.
    if(tag.isEmpty())
        throw Error{HERE, Error::Code::VersionUnparseableError};

    // Semver only allows prerelease tags to contain [0-9A-Za-z-], but we're
    // tolerating anything in the string here.
    _numericValue = fieldToInt(tag);
    // If the field isn't numeric, store the string value
    if(_numericValue == NonNumeric)
        _stringValue = tag.toString();
}

int PrereleaseTag::compare(const PrereleaseTag &other) const
{
    // If both are non-numeric, compare string values
    if(_numericValue == NonNumeric && other._numericValue == NonNumeric)
        return _stringValue.compare(other._stringValue);
    // If both are numeric, compare numeric values
    if(_numericValue != NonNumeric && other._numericValue != NonNumeric)
        return _numericValue - other._numericValue;

    // One is numeric and the other is not.  Non-numeric identifiers have
    // greater precedence.
    return _numericValue == NonNumeric ? 1 : -1;
}

nullable_t<SemVersion> SemVersion::tryParse(const QStringView &version)
{
    // If the version is empty, return null without logging any warning, this is
    // usually normal.
    if(version.isEmpty())
        return {};

    try
    {
        return SemVersion{version};
    }
    catch(const Error &err)
    {
        qWarning() << "Version" << version << "is not valid:" << err;
        return {};
    }
}

SemVersion::SemVersion(const QStringView &version)
    : _major{}, _minor{}, _patch{}
{
    auto majorEnd = indexOf(version, '.');
    if(majorEnd == -1)
        throw Error{HERE, Error::Code::VersionUnparseableError};
    auto minorEnd = indexOf(version, '.', majorEnd+1);
    if(minorEnd == -1)
        throw Error{HERE, Error::Code::VersionUnparseableError};
    // The patch number ends at the end of the string, '+', or '-' - take all
    // digits for now and check later when we parse tags.
    auto patchEnd = minorEnd + 1;
    while(patchEnd < version.size() && version[patchEnd] >= '0' && version[patchEnd] <= '9')
        ++patchEnd;

    // Parse those parts
    _major = PrereleaseTag::fieldToInt({version.begin(), version.begin() + majorEnd});
    _minor = PrereleaseTag::fieldToInt({version.begin() + majorEnd + 1, version.begin() + minorEnd});
    _patch = PrereleaseTag::fieldToInt({version.begin() + minorEnd + 1, version.begin() + patchEnd});
    if(_major == PrereleaseTag::NonNumeric ||
        _minor == PrereleaseTag::NonNumeric ||
        _patch == PrereleaseTag::NonNumeric)
    {
        throw Error{HERE, Error::Code::VersionUnparseableError};
    }

    // Parse tags - prerelease tags followed by build tags
    auto tagStart = patchEnd;
    if(tagStart < version.size() && version[tagStart] == '-')
    {
        // Prerelease tags end either at the end of the string or at the next
        // '+' denoting build tags
        auto prEnd = indexOf(version, '+', tagStart);
        if(prEnd == -1)
            prEnd = version.size();

        // Parse each tag
        while(tagStart < prEnd)
        {
            // Skip the leading '-' (first tag) or '.' (subsequent tags)
            ++tagStart;

            // End at '.' or prEnd
            auto tagEnd = indexOf(version, '.', tagStart);
            if(tagEnd == -1 || tagEnd > prEnd)
                tagEnd = prEnd;

            _prerelease.emplace_back(QStringView{version.begin() + tagStart, version.begin() + tagEnd});
            // Advance to the next tag
            tagStart = tagEnd;
        }
    }

    // At this point, we must either be at build tags or the end of the version.
    // Build tags aren't parsed, but there can't be arbitrary garbage following
    // the patch version.
    if(tagStart < version.size() && version[tagStart] != '+')
        throw Error{HERE, Error::Code::VersionUnparseableError};
}

SemVersion::SemVersion(int major, int minor, int patch)
    : _major(major), _minor(minor), _patch(patch)
{

}

SemVersion::SemVersion(int major, int minor, int patch,
                       std::vector<PrereleaseTag> prerelease)
    : _major{major}, _minor{minor}, _patch{patch},
      _prerelease{std::move(prerelease)}
{
}

bool SemVersion::isPrerelease() const
{
    return !_prerelease.empty();
}

bool SemVersion::isPrereleaseType(const QStringView &type) const
{
    return !_prerelease.empty() && _prerelease[0] == PrereleaseTag{type};
}

int SemVersion::compare(const SemVersion &other) const
{
    // If major, minor, or patch differ, compare those
    if(_major != other._major)
        return _major - other._major;
    if(_minor != other._minor)
        return _minor - other._minor;
    if(_patch != other._patch)
        return _patch - other._patch;

    // Compare prerelease tags
    // If one entry has prerelease tags and the other does not, the
    // non-prerelease version has greater precedence (this is the opposite of
    // the normal case, where more prerelease tags has greater precedence).
    if(_prerelease.empty() && !other._prerelease.empty())
        return 1;
    if(!_prerelease.empty() && other._prerelease.empty())
        return -1;

    // Compare tags lexicographically
    for(auto thisPos = _prerelease.begin(), otherPos = other._prerelease.begin();
        thisPos != _prerelease.end() && otherPos != other._prerelease.end();
        ++thisPos, ++otherPos)
    {
        // If the tags differ, return that comparison
        auto posCompare = thisPos->compare(*otherPos);
        if(posCompare != 0)
            return posCompare;
    }

    // The common tags are all the same.  If one version has additional
    // prerelease tags, it has higher precedence, otherwise the two versions are
    // identical.
    return _prerelease.size() - other._prerelease.size();
}
