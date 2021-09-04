//
// Created by sdk on 8/30/21.
//

#ifndef QUICKOPEN_PLATFORMUTILS_H
#define QUICKOPEN_PLATFORMUTILS_H

#ifdef _MSC_VER
#include "WinUtils.h"
#else
#ifdef __linux__
#include "LinuxUtils.h"
#else
#error "No implementation of platform-specific code is available for this platform."
#endif
#endif

#endif //QUICKOPEN_PLATFORMUTILS_H
