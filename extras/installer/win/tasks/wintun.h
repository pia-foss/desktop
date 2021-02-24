// Copyright (c) 2021 Private Internet Access, Inc.
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

#ifndef TASKS_WINTUN_H
#define TASKS_WINTUN_H

#include "tasks.h"
#include "tap_inl.h"

// These tasks install and uninstall the WinTUN driver package.
//
// The WinTUN driver itself is a shared component, so the WinTUN component is
// shared with any other product that also ships it (the result is that the
// newest version among products shipping the driver is used).
//
// The package shipped by PIA (or the current brand) is specific to that brand
// and contains the shared WinTUN component.  Different brands install/uninstall
// their own package separately, which in turn installs/updates the shared
// WinTUN component.
//
// Because WinTUN is shared, rollbacks of the PIA package may not actually have
// any effect on the installed WinTUN driver (if another product has installed a
// newer driver).
//
// The installer does attempt to downgrade the PIA WinTUN package for a PIA
// downgrade or aborted installation.  It's unlikely, but possible, that the
// component in the package could change in the future, such as if WinTUN broke
// API compatibility and transitioned to a "WinTUN 2" component, etc.

// Uninstall the WinTUN driver.  Only used in uninstaller.
class UninstallWintunTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }
};

// "Install" the WinTUN driver.  Installation is actually deferred until the
// daemon starts up, since MSI packages can't be installed in safe mode, and we
// support installation during safe mode.  This task just creates a flag file
// that tells the daemon to do the installation the next time it starts.
class InstallWintunTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }

private:
    std::wstring _rollbackRunOnceRestore;
};

#endif
