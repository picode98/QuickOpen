//
// Created by sdk on 8/30/21.
//

#ifdef WIN32
#include "WinUtils.cpp"
#else
#ifdef __linux__
#include "LinuxUtils.cpp"
#endif
#endif
