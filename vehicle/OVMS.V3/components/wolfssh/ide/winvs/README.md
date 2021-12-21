VisualStudio solution for wolfSSH
=================================

The solution file, wolfssh.sln, facilitates bulding wolfSSH and its
example and test programs. The solution provides both Debug and Release
builds of Static and Dynamic 32- or 64-bit libraries. The file
`user_settings.h` should be used in the wolfSSL build to configure it.


This project assumes that the wolfSSH and wolfSSL source directories
are installed side-by-side and do not have the version number in their
names:

    Projects\
        wolfssh\
        wolfssl\


The file `wolfssh\ide\winvs\user_settings.h` contains the settings used to
configure wolfSSL with the appropriate settings. This file must be copied
from the directory `wolfssh\ide\winvs` to `wolfssl\IDE\WIN`. If you change
one copy you must change both copies. The option `WOLFCRYPT_ONLY` disables
the build of the wolfSSL files and only builds the wolfCrypt algorithms. To
also keep wolfSSL, delete that option.


User Macros
-----------

The solution is using user macros to indicate the location of the
wolfSSL library and headers. All paths are set to the default build
destinations in the wolfssl64 solution. The user macro `wolfCryptDir`
is used as the base path for finding the libraries. It is initially
set to `..\..\..\..\wolfssl`. And then, for example, the additional
include directories value for the API test project is set to
`$(wolfCryptDir)`.


The wolfCryptDir path must be relative to the project files, which are
all one directory down

    wolfssh/wolfssh.vcxproj
    unit-test/unit-test.vcxproj

etc. The other user macros are the directories where the wolfSSL
libraries for the different builds may be found. So the user macro
`wolfCryptDllRelease64` is initially set to

    $(wolfCryptDir)\x64\DLL Release

This value is used in the debugging environment for the echoserver's
64-bit DLL Release build is set to

    PATH=$(wolfCryptDllRelease64);%PATH%

When you run the echoserver from the debugger, it finds the wolfSSL
DLL in that directory.
