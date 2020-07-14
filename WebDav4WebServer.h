
#ifndef __WEBDAV4WEBSERVER
#define __WEBDAV4WEBSERVER

#include <ESPWebDAV.h>
#include <ESP8266WebServer.h>

extern "C"
ESP8266WebServer::Hook_f hookWebDAVForWebserver (const String& davRootDir, ESPWebDAVCore& dav);

#endif // __WEBDAV4WEBSERVER
