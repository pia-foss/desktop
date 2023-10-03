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

#include <QtTest>
#include <QDir>
#include <kapps_core/src/fs.h>
#include <sys/stat.h>

class tst_core_fs : public QObject
{
    Q_OBJECT

    // Length of a testing directory
    enum { DirLength = 5 };

private:
    // TODO: these helper functions are duplicated in tst_rt_tables_initializer.cpp - fix
    QString randomString(int length) const
    {
        const QString possibleCharacters(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
        QString randomString;

        for(int i = 0; i < length; ++i) {
            int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
            QChar nextChar = possibleCharacters.at(index);
            randomString.append(nextChar);
        }
        return randomString;
    }

    // The parent folder is QDir::tempPath()
    // so it's safe to create this path if necessary
    QString randomDirPath() const
    {
        return (QStringLiteral("%1/%2/%3/%4")
            .arg(QDir::tempPath())
            .arg(randomString(DirLength))
            .arg(randomString(DirLength))
            .arg(randomString(DirLength)));
    }

private slots:
    void testDirExists()
    {
        // When the directory does not exist
        {
            QVERIFY(!kapps::core::fs::dirExists(randomDirPath().toStdString()));
        }
        // When the dir already exists
        {
            QString dirName{QDir::tempPath() + "/" + randomString(DirLength)};
            // Ensure the directory already exists
            // Note we do not use fs::mkDir() here as
            // the mkDir tests rely on dirExists() and
            // that could create a false reality where neither
            // work but our tests say they do - so we use
            // a third-party mkDir() function
            {
                QDir dir;
                dir.mkdir(dirName);
                qInfo() << "Directory name is" << dirName;
            }
            QVERIFY(kapps::core::fs::dirExists(dirName.toStdString()));
        }
    }

    void testMkDirP()
    {
        // When the directory doesn't exist
        {
            std::string nestedDir{randomDirPath().toStdString()};
            QVERIFY(kapps::core::fs::mkDir_p(nestedDir));
            QVERIFY(kapps::core::fs::dirExists(nestedDir));
        }

        // When the directory already exists
        {
            std::string nestedDir{randomDirPath().toStdString()};
            QVERIFY(kapps::core::fs::mkDir_p(nestedDir));
            QVERIFY(
                kapps::core::fs::dirExists(nestedDir)
                && kapps::core::fs::mkDir_p(nestedDir)
            );
        }
    }

    void testCopyFile()
    {
        std::string sourceFilePath;
        {
            QTemporaryFile srcFile;
            srcFile.setAutoRemove(false);
            QVERIFY(srcFile.open());

            QTextStream out(&srcFile);
            out << "hello world";
            out << "goodbye cruel world!";

            sourceFilePath = srcFile.fileName().toStdString();
        }

        // Set an unusual chmod so we can verify
        // perms are maintained in the copy
        ::chmod(sourceFilePath.c_str(), 0765);

        // Get a temporary file name
        // The file is created and then deleted at the end of the block
        // Unfortunately this is the only way of getting a tmp file path
        std::string destFilePath;
        {
            QTemporaryFile destFile;
            QVERIFY(destFile.open());
            destFilePath = destFile.fileName().toStdString();
        }

        // The API we want to test - verify that destFilePath
        // contains the same content as sourceFilePath
        QVERIFY(kapps::core::fs::copyFile(sourceFilePath, destFilePath));

        QFile srcFile{QString::fromStdString(sourceFilePath)};
        QFile destFile{QString::fromStdString(destFilePath)};
        QVERIFY(srcFile.open(QIODevice::ReadOnly) && destFile.open(QIODevice::ReadOnly));
        // Verify the two files contain identical content
        QVERIFY((srcFile.readAll()) == (destFile.readAll()));

        // Verify the mode is also maintained
        struct stat dstInfo{};
        ::stat(destFilePath.c_str(), &dstInfo);

        // We can't just get the result of st_mode as that also includes info
        // about the file type (i.e symlink, regular file, etc) we have to extract
        // out just the permission bits before we do the comparison
        QVERIFY((dstInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))  == 0765);

    }
};

QTEST_GUILESS_MAIN(tst_core_fs)
#include TEST_MOC
