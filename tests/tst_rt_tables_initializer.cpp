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
#include <QtTest>
#include <kapps_core/src/fs.h> // For fs::dirExists() and friends
#include <kapps_net/src/linux/rt_tables_initializer.h>

namespace kapps::net {
class tst_rt_tables_initializer : public QObject
{
    Q_OBJECT

    // Length of a testing directory
    enum { DirLength = 5 };

private:
    // TODO: these helper functions are duplicated in from tst_core_fs.cpp - fix
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

    QString randomTempFilePath() const
    {
        return QDir::tempPath() + "/" + randomString(DirLength);
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
  void testRoutingTableNames()
  {
      RtTablesInitializer rt{"acme", {}};
      QVERIFY(rt.tableNames() == (std::vector<std::string>{"acmevpnrt", "acmevpnOnlyrt", "acmevpnWgrt", "acmevpnFwdrt"}));
  }

  void testUsesEtcRtTablesIfItExists()
  {
      // When etc rt_tables exists but lib rt_tables does not
      {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              etcFile.setAutoRemove(false);
              QVERIFY(etcFile.open());

              // Ensure the etc file has content
              // We will append our own tables to this.
              etcPath = etcFile.fileName();

              {
                  QTextStream out(&etcFile);
                  out << "100\ttable1\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {"libPathNotProvided"}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "100\ttable1\n"
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();
          QCOMPARE(actualContent, expectedContent);
      }

      // When etc rt_tables exists and lib rt_tables - lib should be ignored
      {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              QTemporaryFile libFile;
              etcFile.setAutoRemove(false);
              libFile.setAutoRemove(false);
              QVERIFY(etcFile.open() && libFile.open());

              etcPath = etcFile.fileName();

              {
                  // Ensure the etc file has content
                  // We will append our own tables to this.
                  QTextStream outEtc(&etcFile);
                  outEtc << "100\ttable1\n";

                  // We also add content to the lib file
                  // But this should be ignored - as we preferentially
                  // choose the etc file if it exists
                  QTextStream outLib(&libFile);
                  outLib << "500\ttable2\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {libFile.fileName().toStdString()}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "100\ttable1\n"
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();
          QCOMPARE(actualContent, expectedContent);
      }

      // When etc rt_tables exists and lib rt_tables exist - but etc does not contain a valid index
      // it should get the index from lib
      {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              QTemporaryFile libFile;
              etcFile.setAutoRemove(false);
              libFile.setAutoRemove(false);
              QVERIFY(etcFile.open() && libFile.open());

              etcPath = etcFile.fileName();

              {
                  // note the content doesn't include an index
                  QTextStream outEtc(&etcFile);
                  outEtc << "#table1\n";

                  // We also add content to the lib file
                  // The index should be extracted from here
                  QTextStream outLib(&libFile);
                  outLib << "500\ttable2\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {libFile.fileName().toStdString()}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "#table1\n"
          "501\tpiavpnrt\n"
          "502\tpiavpnOnlyrt\n"
          "503\tpiavpnWgrt\n"
          "504\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();
          QCOMPARE(actualContent, expectedContent);
      }

      // etc file exists but does not contain a valid (numerical) table index
      // it should fall back to the "fallbackIndex" which is 100
      {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              etcFile.setAutoRemove(false);
              QVERIFY(etcFile.open());

              // Ensure the etc file has content
              // We will append our own tables to this.
              etcPath = etcFile.fileName();

              {
                  QTextStream out(&etcFile);
                  out << "not_an_index\ttable1\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {"libPathNotProvided"}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "not_an_index\ttable1\n"
          "100\tpiavpnrt\n"
          "101\tpiavpnOnlyrt\n"
          "102\tpiavpnWgrt\n"
          "103\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();

          // When no valid index can be found, we fallback to an index of 100
          QCOMPARE(actualContent, expectedContent);
      }
  }
  void testEtcDoesNotExist()
  {
      // Neither etc or lib exist
      // it should create the etcFile and append the content
      {
          RtTablesInitializer rt{"pia", {"etcFile", {"libFileDoesNotExist"}} };
          QVERIFY(rt.install());

          QByteArray expectedContent =
          "100\tpiavpnrt\n"
          "101\tpiavpnOnlyrt\n"
          "102\tpiavpnWgrt\n"
          "103\tpiavpnFwdrt\n";

          QFile etcFile{"etcFile"};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();

          // When no valid index can be found, we fallback to an index of 100
          QCOMPARE(actualContent, expectedContent);
      }
  }

  void testEtcDoesNotExistButFallbackDoes()
  {
      // When etc rt_tables does not exist but lib rt_tables does
      // Expected behaviour is we create an etc file and then
      // copy across the seed content from the lib file
      // before then appending the etc file with our tables
      // so etc file content should be: lib file + our content
      {
          QString etcPath = randomTempFilePath();
          QString libPath;
          {
              QTemporaryFile libFile;

              libFile.setAutoRemove(false);
              QVERIFY(libFile.open());

              {
                  QTextStream out(&libFile);
                  out << "100\ttable1\n";
              }

              libPath = libFile.fileName();

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {libPath.toStdString()}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedEtcContent =
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          // Verify etcFile has been created and contains
          // the lib file content + our tables appended.
          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualEtcContent = etcFile.readAll();
          QVERIFY(actualEtcContent == expectedEtcContent);

          // Verify libFile content remains unchanged
          QFile libFile{libPath};
          QVERIFY(libFile.open(QIODevice::ReadOnly));
          auto actualLibContent = libFile.readAll();
          QCOMPARE(actualLibContent, "100\ttable1\n");
      }

      // Same as before, but etcPath lives in a nested folder
      // that hasn't yet been created (this is true on arch where /etc/iproute2/ doesn't necessarily exist)
      // So we test to ensure the nested folder is created as well as the rt_tables file
      {
          QString etcPath;
          QString libPath;
          {
              QTemporaryFile libFile;

              libFile.setAutoRemove(false);
              QVERIFY(libFile.open());

              {
                  QTextStream out(&libFile);
                  out << "100\ttable1\n";
              }

              // Creates a randomized path 3 directories deep (not created on disk)
              QString etcParentFolder{randomDirPath()};
              // Verify this nested path does not exist, so when we later check for the etcPath
              // it will confirm the entire nested path has been created
              QVERIFY(!kapps::core::fs::dirExists(etcParentFolder.toStdString()));
              // Create the full path to the etc rt_tables file
              etcPath = QStringLiteral("%1/rt_tables").arg(etcParentFolder);
              libPath = libFile.fileName();

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {libPath.toStdString()}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedEtcContent =
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          // Verify etcFile has been created and contains
          // the lib file content + our tables appended.
          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualEtcContent = etcFile.readAll();
          QVERIFY(actualEtcContent == expectedEtcContent);

          // Verify libFile content remains unchanged
          QFile libFile{libPath};
          QVERIFY(libFile.open(QIODevice::ReadOnly));
          auto actualLibContent = libFile.readAll();
          QCOMPARE(actualLibContent, "100\ttable1\n");
      }
  }

  void testUsesFallbackPathsInOrder()
  {
      // When etc rt_tables does not exist but share and lib rt_tables do
      // Expected behaviour is we create an etc file and then
      // copy across the seed content from the share file - not the lib file
      // as the share file appears first in the fallback list
      {
          // Generate a random file path
          QString etcPath = randomTempFilePath();
          QString libPath;
          QString sharePath;
          {

              // Share file
              QTemporaryFile shareFile;
              shareFile.setAutoRemove(false);
              QVERIFY(shareFile.open());
              {
                  QTextStream out(&shareFile);
                  out << "100\ttableShare\n";
              }

              // Lib file
              QTemporaryFile libFile;
              libFile.setAutoRemove(false);
              QVERIFY(libFile.open());
              {
                  QTextStream out(&libFile);
                  out << "100\ttableLib\n";
              }

              sharePath = shareFile.fileName();
              libPath = libFile.fileName();

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {sharePath.toStdString(), libPath.toStdString()}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedEtcContent =
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          // Verify etcFile has been created and contains
          // the share file content + our tables appended.
          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualEtcContent = etcFile.readAll();
          QCOMPARE(actualEtcContent, expectedEtcContent);

          // Verify shareFile content remains unchanged
          QFile shareFile{sharePath};
          QVERIFY(shareFile.open(QIODevice::ReadOnly));
          auto actualShareContent = shareFile.readAll();
          QCOMPARE(actualShareContent, "100\ttableShare\n");
      }
  }

  // PIA table indices should start counting
  // from the current highest index + 1
  void testRoutingIndicesCanAppearInAnyOrder()
  {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              etcFile.setAutoRemove(false);
              QVERIFY(etcFile.open());

              // Ensure the etc file has content
              // We will append our own tables to this.
              etcPath = etcFile.fileName();

              {
                  QTextStream out(&etcFile);
                  out << "100\ttable1\n";
                  out << "150\ttable2\n";
                  out << "87\ttable3\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {"libPathNotProvided"}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "100\ttable1\n"
          "150\ttable2\n"
          "87\ttable3\n"
          "151\tpiavpnrt\n"
          "152\tpiavpnOnlyrt\n"
          "153\tpiavpnWgrt\n"
          "154\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();
          QCOMPARE(actualContent, expectedContent);
  }

 // Should only add the tables that don't already exist in the file
  void testShouldOnlyAddMissingTables()
  {
   {
          QString etcPath;
          {
              QTemporaryFile etcFile;
              etcFile.setAutoRemove(false);
              QVERIFY(etcFile.open());

              // Ensure the etc file has content
              // We will append our own tables to this.
              etcPath = etcFile.fileName();

              {
                  QTextStream out(&etcFile);
                  out << "100\ttable1\n";
                  out << "101\tpiavpnrt\n";
                  out << "102\tpiavpnOnlyrt\n";
              }

              RtTablesInitializer rt{"pia", {etcPath.toStdString(), {"libPathNotProvided"}} };
              QVERIFY(rt.install());
          }

          QByteArray expectedContent =
          "100\ttable1\n"
          "101\tpiavpnrt\n"
          "102\tpiavpnOnlyrt\n"
          "103\tpiavpnWgrt\n"
          "104\tpiavpnFwdrt\n";

          QFile etcFile{etcPath};
          QVERIFY(etcFile.open(QIODevice::ReadOnly));
          auto actualContent = etcFile.readAll();
          QCOMPARE(actualContent, expectedContent);
    }
  }
};
}

QTEST_GUILESS_MAIN(kapps::net::tst_rt_tables_initializer)
#include TEST_MOC
