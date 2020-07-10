# WebDAV server library

This is a continuation of @ardyesp's [initial work](https://github.com/ardyesp/ESPWebDAV).

### 3D Printer

This repository focuses on WebDAV protocol.

3D printer informations and examples can be found inside the initial repository.

### WebDAV protocol

Protocol support has been extended thanks to the esp8266/Arduino emulation
environment which allowed to make the porting easier.

Current version has been tested with 
- linux: davfs2
- linux: gvfs/gio (but this one also has issues with an apache webdav server)
- macOS Finder
- windows explorer

Also added / fixed:
- http/1.1 (reusing connections)
- locks, directories
- tested against [litmus test suite](http://www.webdav.org/neon/litmus)
- xml is still not yet properly used<br/>
  (although https://github.com/leethomason/tinyxml2.git works on esp8266)<br/>
  (but that's OK, clients are permissive enough)
- Initial proof of concept with virtual /proc file<br/>
  to reflect some live state without writing any file<br/>
  (best would be to have a virtual FS)

### TODO

- implement credentials
- test MFLN with TLS

### testing

- server: check the `tests/` directory for emulation on host
- litmus: download/build/run also from `tests/` directory<br/>
  for both esp or emulation

### Connect from clients

[doc in example](https://github.com/d-a-v/ESPWebDAV/blob/v2/examples/Simple/Simple.ino#L30-L50)
