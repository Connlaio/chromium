// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SWITCHES_H_
#define TOOLS_GN_SWITCHES_H_

#include <map>

#include "base/strings/string_piece.h"

namespace switches {

struct SwitchInfo {
  SwitchInfo();
  SwitchInfo(const char* short_help,
             const char* long_help);

  const char* short_help;
  const char* long_help;
};

typedef std::map<base::StringPiece, SwitchInfo> SwitchInfoMap;

// Returns the mapping of all global switches.
const SwitchInfoMap& GetSwitches();

// This file contains global switches. If a command takes specific ones only
// to that command, just put them in that command's .cc file.

extern const char kArgs[];
extern const char kArgs_HelpShort[];
extern const char kArgs_Help[];

extern const char kColor[];
extern const char kColor_HelpShort[];
extern const char kColor_Help[];

extern const char kDotfile[];
extern const char kDotfile_HelpShort[];
extern const char kDotfile_Help[];

extern const char kFailOnUnusedArgs[];
extern const char kFailOnUnusedArgs_HelpShort[];
extern const char kFailOnUnusedArgs_Help[];

extern const char kMarkdown[];
extern const char kMarkdown_HelpShort[];
extern const char kMarkdown_Help[];

extern const char kNoColor[];
extern const char kNoColor_HelpShort[];
extern const char kNoColor_Help[];

extern const char kQuiet[];
extern const char kQuiet_HelpShort[];
extern const char kQuiet_Help[];

extern const char kRoot[];
extern const char kRoot_HelpShort[];
extern const char kRoot_Help[];

extern const char kRuntimeDepsListFile[];
extern const char kRuntimeDepsListFile_HelpShort[];
extern const char kRuntimeDepsListFile_Help[];

extern const char kThreads[];
extern const char kThreads_HelpShort[];
extern const char kThreads_Help[];

extern const char kTime[];
extern const char kTime_HelpShort[];
extern const char kTime_Help[];

extern const char kTracelog[];
extern const char kTracelog_HelpShort[];
extern const char kTracelog_Help[];

extern const char kVerbose[];
extern const char kVerbose_HelpShort[];
extern const char kVerbose_Help[];

extern const char kVersion[];
extern const char kVersion_HelpShort[];
extern const char kVersion_Help[];

}  // namespace switches

#endif  // TOOLS_GN_SWITCHES_H_
