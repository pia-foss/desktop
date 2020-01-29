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

#include "common.h"
#line SOURCE_FILE("win_appmanifest.cpp")

#include "win_appmanifest.h"
#include "path.h"
#include <QXmlStreamReader>
#include <QFile>

namespace
{
    const QString manifestNamespace = QStringLiteral("http://schemas.microsoft.com/appx/manifest/foundation/windows10");
}

bool findXmlStartNode(QXmlStreamReader &xmlReader, const QString &nodeNamespace,
                      const QString &nodeName)
{
    do
    {
        if(!xmlReader.readNextStartElement())
        {
            return false;
        }

        qInfo() << "node:" << xmlReader.name() << xmlReader.namespaceUri();

        if(xmlReader.namespaceUri() == nodeNamespace &&
           xmlReader.name() == nodeName)
        {
            // Found the node, we're done
            return true;
        }

        // Otherwise skip it and move on
        xmlReader.skipCurrentElement();
    }
    while(true);
}

bool inspectUwpAppManifest(const QString &installDir, AppExecutables &appExes)
{
    QFile manifestFile{installDir + QStringLiteral("\\appxmanifest.xml")};
    if(!manifestFile.open(QIODevice::OpenModeFlag::ReadOnly))
    {
        qWarning() << "Could not open manifest from directory" << installDir;
        return false;
    }

    QXmlStreamReader manifestXml{&manifestFile};

    // Find the "Package" node (should be the root node)
    if(!findXmlStartNode(manifestXml, manifestNamespace, QStringLiteral("Package")))
    {
        qWarning() << "Failed to find Package node in manifest for" << installDir;
        return false;
    }

    // Find the "Applications" node
    if(!findXmlStartNode(manifestXml, manifestNamespace, QStringLiteral("Applications")))
    {
        qWarning() << "Failed to find Applications node in manifest for" << installDir;
        return false;
    }

    // Find each application
    while(findXmlStartNode(manifestXml, manifestNamespace, QStringLiteral("Application")))
    {
        const auto &attributes = manifestXml.attributes();

        const auto &executable = attributes.value({}, QStringLiteral("Executable"));
        if(!executable.isEmpty())
            appExes.executables.insert(Path{installDir} / executable.toString());
        if(attributes.hasAttribute({}, QStringLiteral("StartPage")))
            appExes.usesWwa = true;
        manifestXml.skipCurrentElement();
    }

    return true;
};
