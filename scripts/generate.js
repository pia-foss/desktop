#!/usr/bin/env node

// Copyright (c) 2019 London Trust Media Incorporated
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

const fs = require('fs')
const recursive = require("recursive-readdir");
const mkdirp = require("mkdirp");
const path = require('path');
const { exec, execSync } = require('child_process');

function writeTextFile(name, text) {
  if (process.platform === 'win32') text = text.replace(/\n/g, '\r\n');
  fs.writeFileSync(name, text);
}

function qmlText (opts) {
  let text = `import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "${opts.appJs}" as App
import "${opts.daemonDir}"

Item {


  Component.onCompleted: {

  }
}
`
return text;
}

function qmlController (opts) {
  let text = `import QtQuick 2.0
import "${opts.daemonDir}"

QtObject {

  Component.onCompleted: {

  }
}
`
return text;
}

function cppHeader(opts) {
  let guard = opts.name.toUpperCase().replace(/[^A-Z0-9_]/g, '_') + '_H';
  return `#include "common.h"
#line HEADER_FILE("${opts.name}.h")

#ifndef ${guard}
#define ${guard}
#pragma once



#endif // ${guard}
`
}
cppHeader.fileExtension = '.h';

function cppSource(opts) {
  return `#include "common.h"
#line SOURCE_FILE("${opts.name}.cpp")

#include "${path.basename(opts.name)}.h"

`
}
cppSource.fileExtension = '.cpp';

function mmSource(opts) {
  return `#include "common.h"
#line SOURCE_FILE("${opts.name}.mm")

#include "${path.basename(opts.name)}.h"

`
}
mmSource.fileExtension = '.mm';

function qobjSourceText(opts) {
  let name = opts.name;
  let nameUpper = opts.name.toUpperCase()
  let nameLower = opts.name.toLowerCase()
  return `#include "common.h"
#include "${nameLower}.h"
#line SOURCE_FILE("${nameLower}.cpp")

${name}::${name}(QObject *parent) : QObject(parent)
{

}
`
}
qobjSourceText.fileExtension = '.cpp';

function qobjHeaderText(opts) {

  let name = opts.name;
  let nameUpper = opts.name.toUpperCase()
  let nameLower = opts.name.toLowerCase()
  return `#include "common.h"

#ifndef ${nameUpper}_H
#define ${nameUpper}_H
#line HEADER_FILE("${nameLower}.h")

#include <QObject>

class ${name}: public QObject
{
    Q_OBJECT
private:

public:
    explicit ${name}(QObject *parent = nullptr);
signals:

public slots:
};

#endif // ${nameUpper}_H
`

}
qobjHeaderText.fileExtension = '.h';

require('yargs')
  .usage('$0 <cmd> [args]')
  .command('qml [name]', 'Generate a QML', (yargs) => {
    yargs.option('name', {
      type: "string"
    });
    yargs.positional('controller', {
      type: "boolean",
      describe: "Generate a controller "
    });
  }, async function (argv) {
    let name = `./client/res/components/${argv.name}.qml`
    let dir = path.dirname(name);
    let jspath = './client/res/javascript/app.js';
    mkdirp.sync(path.dirname(name));
    var generator = argv.controller ? qmlController : qmlText;
    writeTextFile(name, generator({
      appJs: path.relative(dir, './client/res/javascript/app.js'),
      daemonDir: path.relative(dir, './client/res/components/daemon'),
    }));
    execSync(`git add ${name}`);
    console.log("Wrote", name);
  })
  .command('cpp <category> <name>', "Generate CPP/H files", (yargs) => {
    yargs.positional('category', {
      type: 'string',
      choices: ['common', 'client', 'daemon'],
    });
    yargs.positional('name', {
      type: 'string',
      describe: "Basename of .cpp/.h files, optionally with path prefix",
    });
    yargs.option('objc', {
      type: 'boolean',
      describe: "Generate a .mm file instead of a .cpp file",
    });
    yargs.option('qobject', {
      type: 'boolean',
      describe: "Generate a QObject based class",
    });
  }, async function (argv) {
    let basename = `./${argv.category}/src/${argv.name.replace(path.sep, '/')}`.replace(/\.+[^./]*$/, '');
    let generators = [ cppHeader, cppSource ];
    let name = argv.name;
    if(argv.objc) {
      generators = [cppHeader, mmSource];
    }
    if(argv.qobject) {
      generators = [qobjSourceText, qobjHeaderText]
      // ensure lower case all the time
      basename = basename.toLowerCase();
      name = name.split('/').pop();
    }
    generators.forEach(g => {
      writeTextFile(basename + g.fileExtension, g({
        name: name
      }));
      execSync(`git add ${basename + g.fileExtension}`);
      console.log("Wrote", basename + g.fileExtension);
    });
  })
  .help()
  .argv
