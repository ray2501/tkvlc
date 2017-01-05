#!/usr/bin/tclsh
#
# A demo to embed libVLC to TK toolkit widget
#
package require Tk
package require tkvlc

update idletasks

wm title . "tkvlc demo"

option add *tearOff 0
menu .menubar
. configure -menu .menubar
menu .menubar.file
.menubar add cascade -menu .menubar.file -label File

.menubar.file add command -label "Open File" -command {
    set types {
        {{AVI Files}       {.avi}        }
        {{MPG Files}       {.mpg}        }
        {{MP4 Files}       {.mp4}        }
        {{All Files}        *            }
    }
    
    set openfile [tk_getOpenFile -filetypes $types]
    
    if {$openfile != ""} {
        # Fix for Windows, libVLC needs the native file path
        set getFileName [file nativename $openfile]
        tkvlc::open $getFileName
    }
}

.menubar.file add separator
.menubar.file add command -label "Exit" -command {
    if {[tk_messageBox -message "Quit?" -type yesno] eq "yes"} {
       if {[tkvlc::isPlaying]==1} {
          tkvlc::stop	
       }

       tkvlc::destroy
       exit
    }
}

# We'll use a frame control to draw libVLC media player 
set display [frame .tkvlc -width 800 -height 600 -background white -takefocus 1]
pack $display -fill both -expand 1

# initial our embedded libVLC package
tkvlc::init [winfo id $display]

 bind $display <1> {
    if {[tkvlc::isPlaying]==1} {
      tkvlc::pause
    } else {
      tkvlc::play
    }
}

wm protocol . WM_DELETE_WINDOW {
    if {[tk_messageBox -message "Quit?" -type yesno] eq "yes"} {
       if {[tkvlc::isPlaying]==1} {
          tkvlc::stop	
       }

       tkvlc::destroy
       exit
    }
}
