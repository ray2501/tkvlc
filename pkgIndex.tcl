# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded tkvlc 0.8 \
	    [list load [file join $dir libtcl9tkvlc0.8.so] [string totitle tkvlc]]
} else {
    package ifneeded tkvlc 0.8 \
	    [list load [file join $dir libtkvlc0.8.so] [string totitle tkvlc]]
}
