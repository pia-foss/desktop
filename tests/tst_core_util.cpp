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

#include <QtTest>
#include <kapps_core/src/util.h>
#include <kapps_core/src/configwriter.h>

class tst_core_util : public QObject
{
    Q_OBJECT

private slots:
    void testRemoveFile()
    {
        // removeFile() must accept UTF-8 (relevant on Windows where the default
        // code page might not be UTF-8)

        // Use Qt to open a file using a known encoding, then verify that
        // removeFile() is able to delete the same file
        QString tempFilePath;
        {
            QTemporaryFile tempFile{QDir::tempPath() + "/kapps_test_Русский.XXXXXX"};
            tempFile.setAutoRemove(false);
            QVERIFY(tempFile.open());
            tempFilePath = tempFile.fileName();
        }

        QVERIFY(kapps::core::removeFile(tempFilePath.toStdString()));
        QVERIFY(!QFile::exists(tempFilePath));
    }

    void testSplitString()
    {
        using kapps::core::splitString;

        // Empty string
        {
            std::string emptyString;
            QVERIFY(splitString(emptyString, '/').empty());
        }

        // one token to left of delim
        {
            std::string path{"hello/"};
            auto tokens = splitString(path, '/');

            QVERIFY(tokens == (std::vector<std::string>{"hello"}));
        }

        // one token to right of delim
        {
            std::string path{"/hello"};
            auto tokens = splitString(path, '/');

            QVERIFY(tokens == (std::vector<std::string>{"", "hello"}));
        }

        // token on either side of delim
        {
            std::string path{"hello/world"};
            auto tokens = splitString(path, '/');

            QVERIFY(tokens == (std::vector<std::string>{"hello", "world"}));
        }
    }

    void testConfigWriterOpen()
    {
        // Verify that ConfigWriter's constructor accepts UTF-8 (again,
        // relevant on Windows)
        QString tempFilePath{QDir::tempPath() + "/kapps_test_Русский.configwriter"};
        QFile::remove(tempFilePath);
        QVERIFY(!QFile::exists(tempFilePath));
        {
            // Open using UTF-8
            kapps::core::ConfigWriter writer{tempFilePath.toStdString()};
            writer << "test" << writer.endl;
        }
        // Verify existence using UTF-16, this verifies that the open above
        // really used UTF-8
        QVERIFY(QFile::exists(tempFilePath));
    }
};

QTEST_GUILESS_MAIN(tst_core_util)
#include TEST_MOC
