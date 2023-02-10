# WebDAV server library

This "v2" branch is a continuation of @ardyesp's [initial work](https://github.com/ardyesp/ESPWebDAV).
<br/>(the former "v3" branch has been merged into "v2")

### 3D Printer

This repository focuses on WebDAV protocol.

3D printer informations and examples can be found inside the initial repository.

### WebDAV protocol

Protocol support has been extended thanks to the esp8266/Arduino emulation
environment which allowed to make the porting easier.

Current version has been tested with 
- linux: davfs2 (fuse FS)
- linux: gvfs/gio (but this one has issues, also with an apache webdav server)
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
- Integrated with ESP8266WebServer via a hook API.<br/>
  WebDav and Webserver can now live on the same port.<br/>
  Example: Hooked.ino (needs 3.0.0-dev esp8266 core)

### Testing

- an interactive example is provided
- A [test script](tests/run) for emulation on host si provided
- litmus test suite: download/build/run from `tests/` directory<br/>
  for both [esp](tests/run-test-suite) and [emulation](tests/run-test-suite-emu)

### Connect from clients

[Documentation is in the example](examples/Simple/Simple.ino#L30-L50).

### TODO

- implement credentials
- test MFLN with TLS
