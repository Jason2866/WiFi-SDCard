/*
    Copyright (c) 2020 David Gauchard https://github.com/d-a-v/ESPWebDAV
    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
    SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
    OF SUCH DAMAGE.

*/

#include <WebDav4WebServer.h>


#if WEBSERVER_HAS_HOOK
WebServer::HookFunction hookWebDAVForWebserver(const String& davRootDir, ESPWebDAVCore& dav, const String& fsRootDir)
{
    dav.setDAVRoot(davRootDir);
    dav.setFsRoot(fsRootDir);
    return [&dav](const String & method, const String & url, WiFiClient * client, WebServer::ContentTypeFunction contentType)
    {
        if (dav.isIgnored(url))
        {
            DBG_PRINT("CLIENT_REQUEST_CAN_CONTINUE, '%s' is explicitally ignored", url.c_str());
            return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
        }
        if (url.indexOf(dav.getDAVRoot()) != 0)
        {
            DBG_PRINT("CLIENT_REQUEST_CAN_CONTINUE, '%s' is not seen in '%s'", dav.getDAVRoot().c_str(), url.c_str());
            return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
        }
        if (dav.parseRequest(method, url, client, contentType))
        {
            DBG_PRINT("CLIENT_REQUEST_IS_HANDLED ('%s')", url.c_str());
            return WebServer::CLIENT_REQUEST_IS_HANDLED;
        }
        DBG_PRINT("CLIENT_MUST_STOP");
        return WebServer::CLIENT_MUST_STOP;
    };
}
#endif // WEBSERVER_HAS_HOOK
