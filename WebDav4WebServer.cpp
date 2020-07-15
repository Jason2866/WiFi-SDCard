
#include <WebDav4WebServer.h>

ESP8266WebServer::Hook_f hookWebDAVForWebserver (const String& davRootDir, ESPWebDAVCore& dav)
{
    return [&dav, davRootDir](const String& method, const String& url, WiFiClient* client, ESP8266WebServer::ContentType_f contentType)
    {
        int idx = url.indexOf(davRootDir);
        if (idx != 0)
            return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
        if (dav.parseRequest(method, url, client, contentType))
            return ESP8266WebServer::CLIENT_REQUEST_IS_HANDLED;
        else
            return ESP8266WebServer::CLIENT_MUST_STOP;
    };
}
