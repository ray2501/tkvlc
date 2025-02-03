# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded tkvlc 1.0 \
	    [list load [file join $dir libtcl9tkvlc1.0.so] [string totitle tkvlc]]
} else {
    package ifneeded tkvlc 1.0 \
	    [list load [file join $dir libtkvlc1.0.so] [string totitle tkvlc]]
}
