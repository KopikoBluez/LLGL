/*
 * LinuxPath.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "../Path.h"
#include <unistd.h>


namespace LLGL
{

namespace Path
{


LLGL_EXPORT char GetSeparator()
{
    return '/';
}

LLGL_EXPORT UTF8String GetWorkingDir()
{
    char path[PATH_MAX] = { 0 };
    ::getcwd(path, PATH_MAX);
    return UTF8String{ path };
}

LLGL_EXPORT UTF8String GetAbsolutePath(const StringView& filename)
{
    return Combine(GetWorkingDir(), filename);
}


} // /nameapace Path

} // /namespace LLGL



// ================================================================================