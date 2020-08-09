
#include <WebDav4WebServer.h>

ESP8266WebServer::HookFunction hookWebDAVForWebserver(const String& davRootDir, ESPWebDAVCore& dav)
{
    return [&dav, davRootDir](const String & method, const String & url, WiFiClient * client, ESP8266WebServer::ContentTypeFunction contentType)
    {
        return
            url.indexOf(davRootDir) != 0 ? ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE :
            dav.parseRequest(method, url, client, contentType) ? ESP8266WebServer::CLIENT_REQUEST_IS_HANDLED :
            ESP8266WebServer::CLIENT_MUST_STOP;
    };
}
