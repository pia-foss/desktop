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
#line HEADER_FILE("semversion.h")

#ifndef SEMVERSION_H
#define SEMVERSION_H

#include <QString>

// Prerelease tag.  Handles precedence of numeric and non-numeric prerelease
// tags.
class COMMON_EXPORT PrereleaseTag : public Comparable<PrereleaseTag>
{
public:
    enum : int
    {
        // Value used to indicate a non-numeric field
        NonNumeric = -1
    };
    // Parse a field as an integral semver field.  If the field is a valid
    // numeric field (contains only 0-9), returns the non-negative integer
    // value.
    // If the field is not numeric (contains characters other than 0-9),
    // returns NonNumeric.  (This is acceptable for PrereleaseTag.)
    // If the field is numeric but too large to be represented as an int,
    // this throws an Error.
    static int fieldToInt(QStringView field);

public:
    // Construct PrereleaseTag from a single tag from the version.  If the
    // tag is not valid, this throws.
    PrereleaseTag(QStringView tag);
    int compare(const PrereleaseTag &other) const;

private:
    // For a numeric tag, the integer value.  Set to NonNumeric when the tag
    // is not numeric.
    int _numericValue;
    // For a non-numeric tag, the string value.  Meaningful only when
    // _numericValue == NonNumeric.
    QString _stringValue;
};

// SemVersion is a basic model of a semantic version.  It can parse a semantic
// version from a string and order versions by precedence (per the semver spec).
class COMMON_EXPORT SemVersion : public Comparable<SemVersion>
{
public:
    // Attempt to parse a string into a SemVersion - useful when the string
    // might not be valid, such as from settings.json, without breaking
    // SemVersion's invariant.
    // Returns a null nullable_t if the version is not valid.
    // If the version is not empty and can't be parsed, this logs a warning,
    // since this is unexpected for all current uses.  An empty string returns
    // null without logging anything.
    static nullable_t<SemVersion> tryParse(const QStringView &version);

public:
    // Create a SemVersion from a string.  If the string cannot be parsed as a
    // semantic version, this throws.
    // Build tags, if present, are not stored, since they do not impact version
    // precedence.
    SemVersion(const QStringView &version);

    // Create a basic SemVersion from a numeric major, minor, patch triple.
    // This constructor never throws.
    SemVersion(int major, int minor, int patch = 0);

    // Create a basic SemVersion from a numeric major, minor, patch triple, and
    // optional prerelease tags.
    // This constructor never throws (though attempting to construct an invalid
    // PrereleaseTag may throw)
    SemVersion(int major, int minor, int patch,
               std::vector<PrereleaseTag> prerelease);

    // Test if a SemVersion is a prerelease version (it has at least one
    // prerelease tag).
    bool isPrerelease() const;

    // Test if a SemVersion is a particular prerelease type (such as "beta" or
    // "alpha").  A version's prerelease type is defined as its first prerelease
    // tag, if it has any.  ("1.2.0-beta.4" is prerelease type "beta".)
    //
    // (This isn't a concept defined by semantic versioning, it's just the way
    // we define version numbers for the desktop client.)
    bool isPrereleaseType(const QStringView &type) const;

public:
    // Compare versions by precedence.  Returns <0 if this has lower precedence
    // than other, >0 if this has higher precedence than other, or 0 if they are
    // equivalent.
    int compare(const SemVersion &other) const;

    int major() const {return _major;}
    int minor() const {return _minor;}
    int patch() const {return _patch;}

private:
    int _major;
    int _minor;
    int _patch;
    std::vector<PrereleaseTag> _prerelease;
};

#endif
