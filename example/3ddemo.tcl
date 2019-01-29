#!/usr/bin/tclsh
#
# A demo to embed libVLC into a 3D canvas widget

package require Tk
package require Canvas3d
package require tkvlc

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
        tkvlc0 open $openfile
    }
}

.menubar.file add separator
.menubar.file add command -label "Exit" -command exit

if {[info command "sdltk"] eq "sdltk"} {
    wm attributes . -fullscreen 1
}

canvas3d .w1
pack .w1 -expand yes -fill both -side top
.w1 configure -width 640 -height 480 -background black

bind all <Break> exit
bind all <Key-q> exit
bind all <Escape> exit

# Mouse control
bind .w1 <B1-Motion> {
    set ry [expr 360.0 * (%y  - $::Y) / [%W cget -height]]
    set rx [expr 360.0 * (%x  - $::X) / [%W cget -width]]
    %W transform -camera type(light) [list orbitup $ry orbitleft $rx]
    set ::X %x
    set ::Y %y
}

bind .w1 <1> {
    set ::X %x
    set ::Y %y
}

# Mouse wheel for zoom in/out
bind .w1 <5> {%W transform -camera type(light) {movein 0.98}}
bind .w1 <4> {%W transform -camera type(light) {movein 1.02}}

# Rotate around the scene center:
bind .w1 <Up> {%W transform -camera type(light) {orbitup 5.0}}
bind .w1 <Down> {%W transform -camera type(light) {orbitdown 5.0}}
bind .w1 <Left> {%W transform -camera type(light) {orbitright 5.0}}
bind .w1 <Right> {%W transform -camera type(light) {orbitleft 5.0}}

# Zoom in and out:
bind .w1 <Key-s> {%W transform -camera type(light) {movein 0.9}}
bind .w1 <Key-x> {%W transform -camera type(light) {movein 1.1}}

# Rotate camera around line of sight:
bind .w1 <Key-c> {%W transform -camera type(light) {twistright 5.0}}
bind .w1 <Key-z> {%W transform -camera type(light) {twistleft 5.0}}
bind .w1 <Key-y> {%W transform -camera type(light) {twistleft 5.0}}

# Look to the left or right.
bind .w1 <Key-d> {%W transform -camera type(light) {panright 5.0}}
bind .w1 <Key-a> {%W transform -camera type(lignt) {panleft 5.0}}

# Lookat!
bind .w1 <Key-l> {%W transform -camera type(light) {lookat all}}

proc cube {w sidelength tag} {
    set p [expr $sidelength / 2.0]
    set m [expr $sidelength / -2.0]

    $w create polygon [list $p $p $p  $m $p $p  $m $p $m  $p $p $m] -tags x0
    $w create polygon [list $p $m $p  $m $m $p  $m $m $m  $p $m $m] -tags x1

    $w create polygon [list $p $p $p  $p $m $p  $p $m $m  $p $p $m] -tags x2
    $w create polygon [list $m $p $p  $m $m $p  $m $m $m  $m $p $m] -tags x3
}

cube .w1 1.0 cube_one
.w1 create light {0.0 0.0 4.0}
.w1 transform -camera light {lookat all}

proc imagecb {w img event} {
    if {$event ne "frame"} {
	return
    }
    foreach tag {x0 x1 x2 x3} {
	$w itemconfigure $tag -teximage {}
	$w itemconfigure $tag -teximage $img
    }
}

# We'll use a photo image to draw libVLC media player
set photo [image create photo -width 640 -height 480]

# Initialize libVLC
tkvlc::init tkvlc0 $photo

# Frame update to 3d canvas
tkvlc0 event [list imagecb .w1 $photo]
