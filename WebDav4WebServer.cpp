
#include <WebDav4WebServer.h>


#if WEBSERVER_HAS_HOOK
WebServer::HookFunction hookWebDAVForWebserver(const String& davRootDir, ESPWebDAVCore& dav, const String& fsRootDir)
{
    dav.setDAVRoot(davRootDir);
    dav.setFsRoot(fsRootDir);
    return [&dav, davRootDir, fsRootDir](const String & method, const String & url, WiFiClient * client, WebServer::ContentTypeFunction contentType)
    {
        if (url.indexOf(davRootDir) != 0)
        {
            DBG_PRINT("CLIENT_REQUEST_CAN_CONTINUE, %s is not seen in %s", davRootDir.c_str(), url.c_str());
            return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
        }
        if (dav.parseRequest(method, url, client, contentType))
        {
            DBG_PRINT("CLIENT_REQUEST_IS_HANDLED");
            return WebServer::CLIENT_REQUEST_IS_HANDLED;
        }
        DBG_PRINT("CLIENT_MUST_STOP");
        return WebServer::CLIENT_MUST_STOP;
    };
}
#endif // WEBSERVER_HAS_HOOK
