tkvlc
=====

[libVLC] (http://www.videolan.org/vlc/libvlc.html) is the core engine
and the interface to the multimedia framework on which
[VLC media player] (http://www.videolan.org/vlc/) is based.

It is a demo to embed [libVLC] (https://wiki.videolan.org/LibVLC/)
to a [Tk toolkit] (http://www.tcl.tk/) frame widget.

It involves making a frame widget in Tcl, and then passing its Windows
HWND or X Window ID to C (use `winfo id window`) to initialize the
libVLC media player.


License
=====

MIT


Implement commands
=====

tkvlc::init  
tkvlc::open  
tkvlc::play  
tkvlc::pause  
tkvlc::stop  
tkvlc::isPlaying  
tkvlc::destroy


UNIX BUILD
=====

Only test on openSUSE LEAP 42.2 and Ubuntu 14.04.

First is to install libVLC development files. Below is an example for openSUSE:

    sudo zypper in vlc-devel

Below is an example for Ubuntu:

    sudo apt-get install libvlc-dev

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
So if you need specify Tk include, below is an example:

    $ cd tkvlc
    $ ./configure --with-tkinclude=/usr/include/tk
    $ make
    $ make install

Example
=====

Please check example folder.

