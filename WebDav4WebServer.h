
#ifndef __WEBDAV4WEBSERVER
#define __WEBDAV4WEBSERVER

#include <ESPWebDAV.h>
#include <ESP8266WebServer.h>

#if WEBSERVER_HAS_HOOK

extern "C"
ESP8266WebServer::HookFunction hookWebDAVForWebserver(const String& davRootDir, ESPWebDAVCore& dav);

#endif // WEBSERVER_HAS_HOOK

#endif // __WEBDAV4WEBSERVER
