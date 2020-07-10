/*
 * Copyright (c) 2018 Gurpreet Bal https://github.com/ardyesp/ESPWebDAV
 * Copyright (c) 2020 David Gauchard https://github.com/d-a-v/ESPWebDAV
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#ifndef __ESPWEBDAV_H
#define __ESPWEBDAV_H

#if defined(WEBDAV_LOCK_SUPPORT) || defined(DBG_WEBDAV)
#error WEBDAV_LOCK_SUPPORT or DBG_WEBDAV: cannot be defined by user
#endif

// LOCK support is not mandatory
// WEBDAV_LOCK_SUPPORT
// = 0: no support
// = 1: fake support
// > 1: supported with a std::map<>
#define WEBDAV_LOCK_SUPPORT 2

#define DBG_WEBDAV 1

#if CORE_MOCK
#define DBG_WEBDAV 1
#endif

#if !defined(DBG_WEBDAV) && defined(DEBUG_ESP_PORT)
#define DBG_WEBDAV 1
#define DBG_WEBDAV_PORT DEBUG_ESP_PORT
#endif

#if DBG_WEBDAV
// debugging
#ifndef DBG_WEBDAV_PORT
#define DBG_WEBDAV_PORT Serial
#endif
#define DBG_PRINT(...) 	    { DBG_WEBDAV_PORT.print(__VA_ARGS__); }
#define DBG_PRINTF(...)     { DBG_WEBDAV_PORT.printf(__VA_ARGS__); }
#define DBG_PRINTLN(...)    { DBG_WEBDAV_PORT.println(__VA_ARGS__); }
#else
// production
#define DBG_PRINT(...)      { }
#define DBG_PRINTF(...)     { }
#define DBG_PRINTLN(...)    { }
#endif

// constants for WebServer
#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)
#define HTTP_MAX_POST_WAIT 		5000

#if WEBDAV_LOCK_SUPPORT > 1
#include <map>
#endif
#include <functional>
#include <ESP8266WiFi.h>
#include <StreamString.h>

class ESPWebDAV
{
public:

    enum ResourceType { RESOURCE_NONE, RESOURCE_FILE, RESOURCE_DIR };
    enum DepthType { DEPTH_NONE, DEPTH_CHILD, DEPTH_ALL };

    void begin(WiFiServer* srv, FS* fs);
    bool isClientWaiting();
    void handleClient();
    //void rejectClient(const String& rejectMessage);

    static void stripSlashes (String& name);
    static String date2date (time_t date);

    bool dirAction (
        const String& path,
        bool recursive,
        const std::function<bool(int depth, const String& parent, Dir& entry)>& cb,
        bool callAfter = true,
        int depth = 0);

    void dir (const String& path, Print* out);

protected:

    typedef void (ESPWebDAV::*THandlerFunction)(const String&);

    bool copyFile (File file, const String& destName);
    bool deleteDir (const String& dir);

    void processClient(THandlerFunction handler, const String& message);
    void handleIssue (int code, const char* text);
    //void handleReject(const String& rejectMessage);
    void handleRequest(const String& blank);
    void handleOptions(ResourceType resource);
    void handleLock(ResourceType resource);
    void handleUnlock(ResourceType resource);
    void handlePropPatch(ResourceType resource, File& file);
    void handleProp(ResourceType resource, File& file);
    void handleGet(ResourceType resource, File& file, bool isGet);
    void handlePut(ResourceType resource);
    void handleWriteError(const String& message, File& wFile);
    void handleDirectoryCreate(ResourceType resource);
    void handleMove(ResourceType resource, File& file);
    void handleDelete(ResourceType resource);
    void handleCopy(ResourceType resource, File& file);

    void sendPropResponse(bool isDir, const String& name, size_t size, time_t lastWrite, time_t creationTime);
    void sendProp1Response(const String& what, const String& response);

    // Sections are copied from ESP8266Webserver
    String getMimeType(const String& path);
    String urlDecode(const String& text);
    String urlToUri(const String& url);
    bool parseRequest();
    void sendHeader(const String& name, const String& value, bool first = false);
    void send(const String& code, const char* content_type, const String& content);
    void _prepareHeader(String& response, const String& code, const char* content_type, size_t contentLength);
    bool sendContent(const String& content);
    bool sendContent_P(PGM_P content);
    bool sendContent(const char* data, size_t size);
    void setContentLength(size_t len);
    size_t readBytesWithTimeout(uint8_t *buf, size_t size);
    void processRange(const String& range);

    int allowed (const String& uri, uint32_t ownash);
    int allowed (const String& uri, const String& xml = emptyString);
    void makeToken (String& ret, uint32_t pash, uint32_t ownash);
    int extractLockToken (const String& someHeader, const char* start, const char* end, uint32_t& pash, uint32_t& ownash);
    bool getPayload (StreamString& payload);
    void stripName (String& name);

    enum virt_e { VIRT_NONE, VIRT_PROC };
    virt_e isVirtual (const String& uri);
    size_t makeVirtual (virt_e v, String& internal);

    // variables pertaining to current most HTTP request being serviced
    constexpr static int m_persistent_timer_init_ms = 5000;
    long unsigned int m_persistent_timer_ms;
    bool        m_persistent;
    WiFiClient  client;
    WiFiServer* server;
    FS*         gfs;
    int         _maxPathLength;

    StreamString payload;

    String 		method;
    String 		uri;
    size_t 		contentLengthHeader;
    String 		depthHeader;
    String 		hostHeader;
    String		destinationHeader;
    String      overwrite;
    String      ifHeader;
    String      lockTokenHeader;
    DepthType   depth;

    String 		_responseHeaders;
    bool		_chunked;
    int			_contentLengthAnswer;
    int         _rangeStart;
    int         _rangeEnd;

#if WEBDAV_LOCK_SUPPORT > 1
    // infinite-depth exclusive locks
    // map<crc32(path),crc32(owner)>
    std::map<uint32_t, uint32_t> _locks;
#endif
};

#endif // __ESPWEBDAV_H
