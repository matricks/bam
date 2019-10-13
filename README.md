# bam
Bam is a build system with the focus on being having fast build times and flexiable build scripts. Instead of having a custom language it uses Lua to describe the build steps. It's written in clean C and is distrubuted under the liberal zlib licence. Available on many platforms including but not limited to Linux, Mac OS X and Windows.

# Build Status
[![Build Status](https://travis-ci.org/matricks/bam.svg?branch=master)](https://travis-ci.org/wc-duck/bam)
[![Build status](https://ci.appveyor.com/api/projects/status/7foj5473hsnrw8ma?svg=true)](https://ci.appveyor.com/project/wc-duck/bam-bk2ww)

# Quick Taste

This section is a short introduction to bam and is designed to get you started quickly.

```lua
1: settings = NewSettings()
2: settings.cc.defines:Add("MYDEFINE")
3: source = Collect("src/*.c")
5: objects = Compile(settings, source)
4: exe = Link(settings, "my_app", objects)
```

Line 1 creates a new settings object. This contains all the settings on how to compile, link etc.

Line 2 sets a define to be used during compliation.

Line 3 gathers all the files under the src/ directory which has .c as extention. Collect returns a table of strings which are the files.

Line 4 compiles the source using the specified settings and returns a table of the object files.

Line 5 links the object files to an executable named "my_app", using the specified settings.

# Getting it

bam is distributed as source only and downloads can be found at https://github.com/matricks/bam/releases
