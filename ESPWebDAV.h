
#ifndef __ESPWEBDAV_H
#define __ESPWEBDAV_H

#ifdef WEBDAV_LOCK_SUPPORT
#error WEBDAV_LOCK_SUPPORT: cannot be defined by user
#endif

// LOCK support is not robust with davfs2
// LOCK support is not mandatory
// WEBDAV_LOCK_SUPPORT
// = 0: no support
// = 1: fake support
// > 1: supported with a std::map
#define WEBDAV_LOCK_SUPPORT 2

#define DBG_WEBDAV 1

#if DBG_WEBDAV
// debugging
#define DBG_PRINT(...) 	    { Serial.print(__VA_ARGS__); }
#define DBG_PRINTF(...)     { Serial.printf(__VA_ARGS__); }
#define DBG_PRINTLN(...)    { Serial.println(__VA_ARGS__); }
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
    void handleClient(const String& blank = "");
    void rejectClient(const String& rejectMessage);

    static void stripSlashes (String& name);

protected:
    typedef void (ESPWebDAV::*THandlerFunction)(const String&);

    bool dirAction (
        const String& path,
        bool recursive,
        const std::function<bool(int depth, const String& parent, Dir& entry)>& cb,
        int depth = 0);

    void dir (const String& path, Print* out);
    bool copyFile (File file, const String& destName);
    bool deleteDir (const String& dir);

    void processClient(THandlerFunction handler, const String& message);
    void handleIssue (int code, const char* text);
    void handleReject(const String& rejectMessage);
    void handleRequest(const String& blank);
    void handleOptions(ResourceType resource);
    void handleLock(ResourceType resource);
    void handleUnlock(ResourceType resource);
    void handlePropPatch(ResourceType resource, File& file);
    void handleProp(ResourceType resource, File& file);
    void handleGet(ResourceType resource, bool isGet);
    void handlePut(ResourceType resource);
    void handleWriteError(const String& message, File& wFile);
    void handleDirectoryCreate(ResourceType resource);
    void handleMove(ResourceType resource, File& file);
    void handleDelete(ResourceType resource);
    void handleCopy(ResourceType resource, File& file);

    void sendPropResponse(bool isDir, const String& name, size_t size, time_t lastWrite);
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

    bool allowed (const String& uri, uint32_t ownash);
    bool allowed (const String& uri, const String& xml = emptyString);
    void makeToken (String& ret, uint32_t pash, uint32_t ownash);
    void extractLockToken (const String& someHeader, const char* start, const char* end, uint32_t& pash, uint32_t& ownash);
    void getPayload (StreamString& payload);
    void stripName (String& name);

    // variables pertaining to current most HTTP request being serviced
    WiFiServer* server;
    FS*         gfs;
    int         _maxPathLength;

    WiFiClient 	client;
    String 		method;
    String 		uri;
    String 		contentLengthHeader;
    String 		depthHeader;
    String 		hostHeader;
    String		destinationHeader;
    String      overwrite;
    String      ifHeader;
    String      lockTokenHeader;
    DepthType   depth;

    String 		_responseHeaders;
    bool		_chunked;
    int			_contentLength;
    int         _rangeStart;
    int         _rangeEnd;

#if WEBDAV_LOCK_SUPPORT > 1
    // infinite-depth exclusive locks
    // map<crc32(path),crc32(owner)>
    std::map<uint32_t, uint32_t> _locks;
#endif
};

#endif // __ESPWEBDAV_H
