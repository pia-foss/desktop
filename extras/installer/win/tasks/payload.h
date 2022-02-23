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

#ifndef TASKS_PAYLOAD_H
#define TASKS_PAYLOAD_H
#pragma once

#include "tasks.h"
#include "file.h"
#include "list.h"

#ifdef INSTALLER

// Load the payload from the installer executable.
bool initializePayload();

#include "7z.h"

struct CMemoryInStream
{
    ILookInStream vt;
    const Byte *buf;
    size_t size;
    size_t pos;

    static CMemoryInStream* fromVTable(const ILookInStream* pp) { return const_cast<CMemoryInStream*>(reinterpret_cast<const CMemoryInStream*>(reinterpret_cast<const char*>(pp) - offsetof(CMemoryInStream, vt))); }
};

// Task to extract the payload of the installer.
class PayloadTask : public TaskList
{
    friend class PayloadExtractTask;
private:
    static constexpr double DecompressBytesPerSecond = 20000000.0;
    static constexpr double WriteBytesPerSecond = 20000000.0;

    class UnpackTask : public Task
    {
    public:
        UnpackTask(UInt32 folderIndex, size_t folderSize);
        inline PayloadTask& parent() { return *static_cast<PayloadTask*>(_listener); }
        virtual void execute() override;
        virtual double getEstimatedExecutionTime() const override { return _folderSize / DecompressBytesPerSecond; }
        virtual double getEstimatedRollbackTime() const override { return 0.0; }
        void inputStreamPosition(size_t offset, size_t size);
    private:
        UInt32 _folderIndex;
        size_t _folderSize;
        size_t _inputStreamOffset, _inputStreamLength;
    };
    class ExtractTask : public CreateFileTask
    {
    public:
        ExtractTask(std::wstring path, UInt32 fileIndex, size_t offset, size_t size);
        inline PayloadTask& parent() { return *static_cast<PayloadTask*>(_listener); }
        virtual void execute() override;
        virtual double getEstimatedExecutionTime() const override { return 0.005 + _size / WriteBytesPerSecond; }
    private:
        UInt32 _fileIndex;
        size_t _offset, _size;
    };
public:
    PayloadTask(std::wstring installPath);
    ~PayloadTask();
    virtual void prepare() override;
    virtual void execute() override;

    static void notifyInputStreamPosition(size_t offset, size_t size);
private:
    std::wstring _installPath;
    ISzAlloc _alloc, _allocTemp;
    CSzArEx _db;
    CMemoryInStream _stream;
    Byte* _buffer = nullptr;
    size_t _bufferSize = 0;

    static UnpackTask* _currentUnpackTask;
};

#endif // INSTALLER

#endif // TASKS_PAYLOAD_H
