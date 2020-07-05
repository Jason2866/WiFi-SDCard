gvfs: gio mount http://.... -> /run/user/uid/gvfs/dav:...

- gvfs does not like empty FS
- add /proc file
- linux: mplayer does not work on gvfs (but cp does work)
- windows explorer seems ok r/w - to be rechecked
- macOS Finder is ok with reading, writing has to be rechecked
- FS full is detected locally, but the error is not well propagated
- http/1.1 to honour (currently: http/1.1 but "connection: closed")
- test with SSL
- test with SSL+MFLN
- test with credentials
