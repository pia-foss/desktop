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

#include "payload.h"
#ifdef INSTALLER

// Payload data (for installer)
static HRSRC g_payloadResource = NULL;
static HGLOBAL g_payloadHandle = NULL;
static const BYTE* g_payloadData = nullptr;
static DWORD g_payloadSize = 0;

bool initializePayload()
{
    // Load the payload if it exists
    if (g_payloadResource = FindResource(g_instance, MAKEINTRESOURCE(IDR_PAYLOAD), RT_RCDATA))
    {
        g_payloadSize = SizeofResource(g_instance, g_payloadResource);
        if (g_payloadHandle = LoadResource(g_instance, g_payloadResource))
        {
            g_payloadData = (unsigned char*)LockResource(g_payloadHandle);
        }
    }
    return g_payloadData != nullptr;
}

#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"

WRes InFile_Open(CSzFile *p, const WCHAR *name);
WRes OutFile_Open(CSzFile *p, const WCHAR *name);

SRes MemoryInStream_Look(const ILookInStream *pp, const void **buf, size_t *size);
SRes MemoryInStream_Skip(const ILookInStream *pp, size_t offset);
SRes MemoryInStream_Read(const ILookInStream *pp, void *buf, size_t *size);
SRes MemoryInStream_Seek(const ILookInStream *pp, Int64 *pos, ESzSeek origin);
void MemoryInStream_CreateVTable(CMemoryInStream *p, const Byte* buf, size_t size);


WRes InFile_Open(CSzFile* p, const WCHAR* name)
{
    return InFile_OpenW(p, name);
}
WRes OutFile_Open(CSzFile* p, const WCHAR* name)
{
    return OutFile_OpenW(p, name);
}

SRes MemoryInStream_Look(const ILookInStream* pp, const void** buf, size_t* size)
{
    auto p = CMemoryInStream::fromVTable(pp);
    size_t rem = p->size - p->pos;
    if (*size > rem)
        *size = rem;
    *buf = p->buf + p->pos;
    PayloadTask::notifyInputStreamPosition(p->pos, *size);
    return SZ_OK;
}

SRes MemoryInStream_Skip(const ILookInStream* pp, size_t offset)
{
    auto p = CMemoryInStream::fromVTable(pp);
    p->pos += offset;
    if (p->pos > p->size)
        p->pos = p->size;
    return SZ_OK;
}

SRes MemoryInStream_Read(const ILookInStream* pp, void* buf, size_t* size)
{
    auto p = CMemoryInStream::fromVTable(pp);
    size_t read = p->size - p->pos;
    if (read > *size)
        read = *size;
    memcpy(buf, p->buf + p->pos, read);
    p->pos += read;
    *size = read;
    return SZ_OK;
}

SRes MemoryInStream_Seek(const ILookInStream* pp, Int64* pos, ESzSeek origin)
{
    auto p = CMemoryInStream::fromVTable(pp);
    switch (origin)
    {
    case SZ_SEEK_SET: p->pos = *pos; if (p->pos > p->size) p->pos = *pos < 0 ? 0 : p->size; break;
    case SZ_SEEK_CUR: p->pos += *pos; if (p->pos > p->size) p->pos = *pos < 0 ? 0 : p->size; break;
    case SZ_SEEK_END: p->pos = p->size - *pos; if (p->pos > p->size) p->pos = *pos > 0 ? 0 : p->size; break;
    }
    *pos = p->pos;
    return SZ_OK;
}

void MemoryInStream_CreateVTable(CMemoryInStream* p, const Byte* buf, size_t size)
{
    p->vt.Look = &MemoryInStream_Look;
    p->vt.Skip = &MemoryInStream_Skip;
    p->vt.Read = &MemoryInStream_Read;
    p->vt.Seek = &MemoryInStream_Seek;
    p->buf = buf;
    p->size = size;
    p->pos = 0;
}


PayloadTask::UnpackTask* PayloadTask::_currentUnpackTask = nullptr;

PayloadTask::UnpackTask::UnpackTask(UInt32 folderIndex, size_t folderSize)
    : _folderIndex(folderIndex), _folderSize(folderSize)
{

}

void PayloadTask::UnpackTask::execute()
{
    LOG("Unpacking folder %d", _folderIndex);

    _listener->setCaption(IDS_CAPTION_UNPACKING);

    auto& parent = this->parent();

    ISzAlloc_Free(&parent._alloc, parent._buffer);
    parent._buffer = NULL;
    if (_folderSize > 0)
    {
        if (!(parent._buffer = (Byte*)ISzAlloc_Alloc(&parent._alloc, _folderSize)))
            InstallerError::abort(IDS_MB_OUTOFMEMORY);
        parent._bufferSize = _folderSize;

        // Make note of which part of the input stream will be read; this way we
        // can intercept the stream operations to deduce progress.
        auto packs = parent._db.db.PackPositions + parent._db.db.FoStartPackStreamIndex[_folderIndex];
        _inputStreamOffset = packs[0] + parent._db.dataPos;
        _inputStreamLength = packs[1] - packs[0];

        // Decode the LZMA folder
        _currentUnpackTask = this;
        SRes err = SzAr_DecodeFolder(&parent._db.db, _folderIndex, &parent._stream.vt, parent._db.dataPos, parent._buffer, parent._bufferSize, &parent._allocTemp);
        _currentUnpackTask = nullptr;
        if (err)
        {
            LOG("Payload decode error %d", err);
            InstallerError::abort(IDS_MB_PAYLOADDECOMPRESSIONERROR);
        }
    }
}

void PayloadTask::UnpackTask::inputStreamPosition(size_t offset, size_t size)
{
    if (_inputStreamLength > 0)
    {
        double progress = (double)(offset - _inputStreamOffset) / _inputStreamLength;
        if (progress < 0.0)
            progress = 0.0;
        if (progress > 1.0)
            progress = 1.0;
        _listener->setProgress(progress, (1.0 - progress) * getEstimatedExecutionTime());
    }
}

PayloadTask::ExtractTask::ExtractTask(std::wstring path, UInt32 fileIndex, size_t offset, size_t size)
    : CreateFileTask(std::move(path)), _fileIndex(fileIndex), _offset(offset), _size(size)
{

}

void PayloadTask::ExtractTask::execute()
{
    LOG("Extracting file %s", _path);

    _listener->setCaption(IDS_CAPTION_COPYINGFILES);

    const auto& parent = this->parent();

    if (SzBitWithVals_Check(&parent._db.CRCs, _fileIndex))
        if (CrcCalc(parent._buffer + _offset, _size) != parent._db.CRCs.Vals[_fileIndex])
            InstallerError::abort(IDS_MB_CORRUPTPAYLOADCRC);

    CreateFileTask::execute();

    CSzFile outFile;
    if (WRes err = OutFile_Open(&outFile, _path.c_str()))
        InstallerError::abort(UIString{IDS_MB_UNABLETOCREATEFILE, _path});

    size_t writtenBytes = _size;
    if (WRes err = File_Write(&outFile, parent._buffer + _offset, &writtenBytes))
    {
        File_Close(&outFile);
        InstallerError::abort(UIString{IDS_MB_UNABLETOWRITEFILE, _path});
    }
    else if (writtenBytes != _size)
    {
        File_Close(&outFile);
        InstallerError::abort(UIString{IDS_MB_UNABLETOWRITEENTIREFILE, _path});
    }

#ifdef USE_WINDOWS_FILE
    if (SzBitWithVals_Check(&parent._db.MTime, _fileIndex))
    {
        const CNtfsFileTime *t = parent._db.MTime.Vals + _fileIndex;
        FILETIME mTime;
        mTime.dwLowDateTime = t->Low;
        mTime.dwHighDateTime = t->High;
        SetFileTime(outFile.handle, NULL, NULL, &mTime);
    }
#endif

    File_Close(&outFile);

#ifdef USE_WINDOWS_FILE
    if (SzBitWithVals_Check(&parent._db.Attribs, _fileIndex))
        SetFileAttributes(_path.c_str(), parent._db.Attribs.Vals[_fileIndex]);
#endif
}

PayloadTask::PayloadTask(std::wstring installPath)
    : _installPath(std::move(installPath))
{
    _alloc.Alloc = &SzAlloc;
    _alloc.Free = &SzFree;
    _allocTemp.Alloc = &SzAllocTemp;
    _allocTemp.Free = &SzFreeTemp;

    CrcGenerateTable();

    SzArEx_Init(&_db);

    // Wrap a memory stream around the payload resource
    MemoryInStream_CreateVTable(&_stream, g_payloadData, g_payloadSize);
}

PayloadTask::~PayloadTask()
{
    ISzAlloc_Free(&_alloc, _buffer);

    SzArEx_Free(&_db, &_alloc);
}

void PayloadTask::prepare()
{
    // Open the payload archive
    if (WRes err = SzArEx_Open(&_db, &_stream.vt, &_alloc, &_allocTemp))
    {
        LOG("Decompressor initialization error %d", err);
        InstallerError::abort(IDS_MB_DECOMPRESSORINITERROR);
    }

    // Batch up all required directories upfront so we get early failures
    // if something can't be moved out of the way. Be sure to still record
    // the uninstall actions in order though.
    std::set<std::wstring> directories;
    auto addDirectory = [this, &directories](std::wstring path) {
        auto p = directories.insert(std::move(path));
        if (p.second)
            recordUninstallAction("DIR", *p.first);
    };
    auto& directoryTasks = addNew<TaskList>();

    UInt32 lastFolderIndex = (UInt32)-1;
    size_t folderOffset = 0;
    size_t folderSize = 0;

    recordInstallationSize((size_t)_db.UnpackPositions[_db.NumFiles]);

    // Parse all files in payload
    for (UInt32 fileIndex = 0; fileIndex < _db.NumFiles; fileIndex++)
    {
        UInt32 folderIndex = _db.FileToFolder[fileIndex];
        if (folderIndex != lastFolderIndex)
        {
            folderSize = (size_t)SzAr_GetFolderUnpackSize(&_db.db, folderIndex);
            folderOffset = _db.UnpackPositions[_db.FolderToFile[folderIndex]];
            addNew<UnpackTask>(folderIndex, folderSize);
            lastFolderIndex = folderIndex;
        }

        std::wstring relativePath;
        relativePath.resize(SzArEx_GetFileNameUtf16(&_db, fileIndex, NULL), 0);
        SzArEx_GetFileNameUtf16(&_db, fileIndex, (UInt16*)&relativePath[0]);
        relativePath.pop_back(); // extra null

        for (size_t i = 0; i < relativePath.size(); i++)
        {
            if (relativePath[i] == '/')
            {
                addDirectory(relativePath.substr(0, i));
                // Replace with proper Windows directory separator
                relativePath[i] = '\\';
            }
        }
        if (SzArEx_IsDir(&_db, fileIndex))
        {
            addDirectory(relativePath);
            continue;
        }

        std::wstring path = _installPath + L"\\" + relativePath;

        size_t fileOffset = _db.UnpackPositions[fileIndex];
        size_t fileSize = _db.UnpackPositions[fileIndex + 1] - fileOffset;
        size_t fileOffsetInBuffer = fileOffset - folderOffset;

        if (fileOffsetInBuffer + fileSize > folderSize)
            InstallerError::abort(IDS_MB_CORRUPTPAYLOADPARAMS);

        recordUninstallAction("FILE", std::move(relativePath));

        addNew<ExtractTask>(std::move(path), fileIndex, fileOffsetInBuffer, fileSize);
    }

    for (const auto& dir : directories)
        directoryTasks.addNew<CreateDirectoryTask>(_installPath + L"\\" + dir);

    TaskList::prepare();
}

void PayloadTask::execute()
{
    LOG("Extracting payload");

    TaskList::execute();
    ISzAlloc_Free(&_alloc, _buffer);
    _buffer = NULL;
}

void PayloadTask::notifyInputStreamPosition(size_t offset, size_t size)
{
    if (_currentUnpackTask)
        _currentUnpackTask->inputStreamPosition(offset, size);
}

#endif // INSTALLER
