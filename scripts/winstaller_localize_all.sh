#! /usr/bin/env bash

# Copyright (c) 2022 Private Internet Access, Inc.
#
# This file is part of the Private Internet Access Desktop Client.
#
# The Private Internet Access Desktop Client is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# The Private Internet Access Desktop Client is distributed in the hope that
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Private Internet Access Desktop Client.  If not, see
# <https://www.gnu.org/licenses/>.

# CD into this script's directory to run winstaller_localize.js
scriptsDir="${BASH_SOURCE%/*}"
cd ${scriptsDir}

# Windows does not have a "neutral" language code for Arabic.  We only support one Arabic translation,
# and this is never presented in the UI, so the specific country used here doesn't matter.
./winstaller_localize.js ar      LANG_ARABIC              SUBLANG_ARABIC_SAUDI_ARABIA  ARABIC_CHARSET --mirror
./winstaller_localize.js da      LANG_DANISH              SUBLANG_DANISH_DENMARK       ANSI_CHARSET
./winstaller_localize.js de      LANG_GERMAN              SUBLANG_GERMAN               ANSI_CHARSET
./winstaller_localize.js es_MX   LANG_SPANISH             SUBLANG_SPANISH_MEXICAN      ANSI_CHARSET
./winstaller_localize.js fr      LANG_FRENCH              SUBLANG_FRENCH               ANSI_CHARSET
./winstaller_localize.js it      LANG_ITALIAN             SUBLANG_ITALIAN              ANSI_CHARSET
./winstaller_localize.js ko      LANG_KOREAN              SUBLANG_KOREAN               HANGUL_CHARSET
./winstaller_localize.js nb      LANG_NORWEGIAN           SUBLANG_NORWEGIAN_BOKMAL     ANSI_CHARSET
./winstaller_localize.js nl      LANG_DUTCH               SUBLANG_DUTCH                ANSI_CHARSET
./winstaller_localize.js pl      LANG_POLISH              SUBLANG_POLISH_POLAND        ANSI_CHARSET
./winstaller_localize.js pt_BR   LANG_PORTUGUESE          SUBLANG_PORTUGUESE_BRAZILIAN ANSI_CHARSET
./winstaller_localize.js ru      LANG_RUSSIAN             SUBLANG_RUSSIAN_RUSSIA       RUSSIAN_CHARSET
./winstaller_localize.js th      LANG_THAI                SUBLANG_THAI_THAILAND        THAI_CHARSET
./winstaller_localize.js tr      LANG_TURKISH             SUBLANG_TURKISH_TURKEY       ANSI_CHARSET
./winstaller_localize.js ja      LANG_JAPANESE            SUBLANG_JAPANESE_JAPAN       SHIFTJIS_CHARSET
./winstaller_localize.js zh_Hans LANG_CHINESE_SIMPLIFIED  SUBLANG_CHINESE_SIMPLIFIED   GB2312_CHARSET
./winstaller_localize.js zh_Hant LANG_CHINESE_TRADITIONAL SUBLANG_CHINESE_TRADITIONAL  CHINESEBIG5_CHARSET

# Pseudotranslation - set Windows app language to Romanian or Pashto to see
# these in debug installer builds.
# Use SHIFTJIS_CHARSET so the katakana test character will render.  This uses
# a system font, since Roboto doesn't have Japanese.
./winstaller_localize.js ro      LANG_ROMANIAN            SUBLANG_ROMANIAN_ROMANIA     SHIFTJIS_CHARSET --debug
./winstaller_localize.js ps      LANG_PASHTO              SUBLANG_PASHTO_AFGHANISTAN   SHIFTJIS_CHARSET --mirror --debug
