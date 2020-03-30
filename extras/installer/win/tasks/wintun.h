// Copyright (c) 2020 Private Internet Access, Inc.
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

class WintunTask : public Task
{
private:
    static bool _loadedProducts;
    static std::vector<std::wstring> _installedProducts;
    static WinDriverVersion _installedVersion;

protected:
    // Reinstall on rollback is implemented by the uninstall task, but in some
    // cases must be flagged by the install task (for a regular in-place upgrade
    // of the WinTUN package)
    static bool _rollbackReinstall;

public:
    static void loadProducts();
    static const WinDriverVersion &getInstalledVersion();
    static const std::vector<std::wstring> &getInstalledProducts();

protected:
    // Uninstall a WinTUN package specified by the installed product code.
    // Traces result, returns false for failure.
    bool uninstallProduct(const wchar_t *pProductCode);
    // Install the WinTUN package.  Traces result, returns false for failure.
    bool installPackage();

public:
    using Task::Task;
};

// Uninstall the WinTUN driver.  In the installer, only uninstalls if the
// installed version is different from the shipped version.
class UninstallWintunTask : public WintunTask
{
public:
    using WintunTask::WintunTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }
};

class InstallWintunTask : public WintunTask
{
public:
    using WintunTask::WintunTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }

private:
    bool _rollbackUninstall{false};
};

#endif
