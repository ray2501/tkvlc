#!/usr/bin/tclsh
#
# A demo to embed libVLC to TK toolkit widget
#
package require Tk
package require tkvlc

update idletasks

proc scaleVolume {mywidget scaleValue} {
    tkvlc0 setVolume [$mywidget get]
}

proc quitApp {} {
    if {[tk_messageBox -message "Quit?" -type yesno] eq "yes"} {
       if {[tkvlc0 isPlaying]==1} {
          tkvlc0 stop
       }

       tkvlc0 destroy
       exit
    }
}

wm title . "tkvlc demo"

option add *tearOff 0
menu .menubar
. configure -menu .menubar
menu .menubar.file
menu .menubar.tool
.menubar add cascade -menu .menubar.file -label File

.menubar.file add command -label "Open File" -command {
    set types {
        {{AVI Files}       {.avi}        }
        {{3GP Files}       {.3gp}        }
        {{FLV Files}       {.flv}        }
        {{MKF Files}       {.mkv}        }
        {{MPG Files}       {.mpg}        }
        {{MP4 Files}       {.mp4}        }
        {{All Files}        *            }
    }

    set openfile [tk_getOpenFile -filetypes $types -multiple 0]
    
    if {$openfile != ""} {
        # Fix for Windows, libVLC needs the native file path
        set getFileName [file nativename $openfile]
        tkvlc0 open $getFileName
    }
}

.menubar.file add separator
.menubar.file add command -label "Exit" -command quitApp

.menubar add cascade -menu .menubar.tool -label Tool

.menubar.tool add command -label "Volume" -command {
  set vw [toplevel .volume]
  wm title $vw "Setup"

  scale .volume.scale -orient vertical -length 450 -from 100 -to 0 \
    -showvalue 1 -tickinterval 20 -command "scaleVolume .volume.scale"
  grid .volume.scale -row 0 -column 0 -sticky ne
  set value [tkvlc0 getVolume]
  .volume.scale set $value

  wm protocol $vw WM_DELETE_WINDOW {
    destroy .volume
  }
}
.menubar.tool add command -label "libVLC version" -command {
  tk_messageBox -message "libVLC version: [tkvlc0 version]" -type ok
}

# We'll use a frame control to draw libVLC media player 
set display [frame .tkvlc -width 800 -height 600 -background white -takefocus 1]
pack $display -fill both -expand 1

# initial our embedded libVLC package
tkvlc::init tkvlc0 [winfo id $display]

 bind $display <1> {
    if {[tkvlc0 isPlaying]==1} {
      tkvlc0 pause
    } else {
      tkvlc0 play
    }
}

wm protocol . WM_DELETE_WINDOW quitApp

