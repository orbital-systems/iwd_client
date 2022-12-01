# iwd_client
Client library towards wifi controller iwd

We needed something that could talk DBUS and manage everything with regards being a WiFi station (client) in an embedded IoT systemet using iwd as WiFi daemon. This lib can connect, forget and scan. It handles all DBUS connections and is based on libell (same as iwd is). It has a small footprint and is fully asynchorious, using simple callbacks when operations are done. It uses libell main loop.

I did ask the iwd mailing list once if something like this exists, but it didn't. I was later asked if we did implement something and if we cna share it. Sure we did, and sure we can. It is not under LGPL as one woudl execpt, but rather MIT-license. THis is because we have not made a library of it, and we do not want to spend the time on doing it right now. We want others to be able to use it.

This is also the reason why there is no Makefile or build system as this is just lose files from a larger project. It should be fairly okay documented inside each source file. Sadly there is no example usage.
