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

#include <common/src/common.h>
#line HEADER_FILE("authcommand.h")

#ifndef AUTHCOMMAND_H
#define AUTHCOMMAND_H

#include "clicommand.h"

// Read a credential file consisting of newline-delimited credential components
// (user/password, tokens, etc.).  Trailing CRs are trimmed if present.  If the
// file can't be opened, throws CliInvalidArgs.
QStringList readCredentialFile(const QString &filePath);

class LoginCommand : public CliCommand
{
public:
    virtual void printHelp(const QString &name) override;
    virtual int exec(const QStringList &params, QCoreApplication &app) override;
};


#endif
