tkvlc
=====

[libVLC](http://www.videolan.org/vlc/libvlc.html) is the core engine
and the interface to the multimedia framework on which
[VLC media player](http://www.videolan.org/vlc/) is based.

It is a demo to embed [libVLC](https://wiki.videolan.org/LibVLC/)
to a [Tk toolkit](http://www.tcl.tk/) frame widget.

User can use `--with-tk-photo` flag to disable/enable to allow for photo images.
This feature "allow for photo images" is from 
[AndroWish](https://www.androwish.org/index.html/home) project
and written by Christian Werner. Default is yes.

It involves making a frame widget in Tcl, and then passing its Windows
HWND or X Window ID to C (use `winfo id window`) to initialize the
libVLC media player.

User also can use this extension in console program, just not pass HWND or photo 
when using `tkvlc::init` to initialize (after version 0.4).
However, tkvlc in console program now only work when interactive with tclsh.


License
=====

MIT


Implement commands
=====

tkvlc::init HANDLE ?HWND|photo?  
HANDLE open filename  
HANDLE play  
HANDLE pause  
HANDLE stop  
HANDLE isplaying  
HANDLE volume ?value?  
HANDLE duration  
HANDLE time ?value?  
HANDLE position ?value?
HANDLE isseekable  
HANDLE state  
HANDLE rate ?value?  
HANDLE version  
HANDLE destroy

`duration` get duration (in second) of movie time.

`getTime` get the current movie time (in second).

`setTime` set the movie time (in second).

`getPosition` get movie position as percentage between 0.0 and 1.0.

`setPosition` set movie position as percentage between 0.0 and 1.0.

`isSeekable` return true if the media player can seek.

`getState` get current movie state.

Movie state has below states (string): idle, opening, buffering, playing,
paused, stopped, ended and error


UNIX BUILD
=====

Only test on openSUSE LEAP 42.2 and Ubuntu 14.04.

First is to install libVLC development files. Below is an example for openSUSE:

    sudo zypper in vlc-devel

Below is an example for Ubuntu:

    sudo apt-get install libvlc-dev

And please notice, I think user still needs VLC codec package when playing a multimedia file.

Building under most UNIX systems is easy, just run the configure script
and then run make. For more information about the build process, see
the tcl/unix/README file in the Tcl src dist. The following minimal
example will install the extension in the /opt/tcl directory.

    $ cd tkvlc
    $ ./configure --prefix=/opt/tcl
    $ make
    $ make install

If you need setup directory containing tcl configuration (tclConfig.sh),
below is an example:

    $ cd tkvlc
    $ ./configure --with-tcl=/opt/activetcl/lib
    $ make
    $ make install

I enable `TEA_PROG_WISH` check, because this extension needs Tk toolkit.
So if you need specify Tk include path, below is an example:

    $ cd tkvlc
    $ ./configure --with-tkinclude=/usr/include/tk
    $ make
    $ make install

WINDOWS BUILD
=====

## MSYS2/MinGW-W64

VLC binary installers for Windows do not include the LibVLC SDK.

To build this extension, user needs libVLC import libraries and development headers.
Please check [LibVLC_Tutorial](https://wiki.videolan.org/LibVLC_Tutorial/#Windows).
Copy libraries to C:\msys64\mingw64\lib and headers (vlc folder) to C:\msys64\mingw64\include.

When execute tkvlc, user needs libvlc.dll, libvlccore.dll and plugins files.

Next step is to build tkvlc.

    $ ./configure --with-tcl=/c/tcl/lib
    $ make
    $ make install

Example
=====

Please check example folder.

