
#include <functional>

#include <ESP8266WiFi.h>

// debugging
#define DBG_PRINT(...) 		{ Serial.print(__VA_ARGS__); }
#define DBG_PRINTF(...) 	{ Serial.printf(__VA_ARGS__); }
#define DBG_PRINTLN(...) 	{ Serial.println(__VA_ARGS__); }
// production
//#define DBG_PRINT(...)    { }
//#define DBG_PRINTF(...)   { }
//#define DBG_PRINTLN(...) 	{ }

// constants for WebServer
#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)
#define HTTP_MAX_POST_WAIT 		5000

// must be fixed: https://github.com/nextcloud/server/issues/17275#issuecomment-535501157

class ESPWebDAV
{
public:
    enum ResourceType { RESOURCE_NONE, RESOURCE_FILE, RESOURCE_DIR };
    enum DepthType { DEPTH_NONE, DEPTH_CHILD, DEPTH_ALL };

    void begin(WiFiServer* srv, FS* fs);
    bool isClientWaiting();
    void handleClient(const String& blank = "");
    void rejectClient(const String& rejectMessage);

    static void stripSlashes (String& name, bool front = false);

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

    void sendPropResponse(bool isDir, const char* name, size_t size, time_t lastWrite);

    // Sections are copied from ESP8266Webserver
    String getMimeType(const String& path);
    String urlDecode(const String& text);
    String urlToUri(const String& url);
    bool parseRequest();
    void sendHeader(const String& name, const String& value, bool first = false);
    void send(const String& code, const char* content_type, const String& content);
    void _prepareHeader(String& response, const String& code, const char* content_type, size_t contentLength);
    void sendContent(const String& content);
    void sendContent_P(PGM_P content);
    void setContentLength(size_t len);
    size_t readBytesWithTimeout(uint8_t *buf, size_t size);
    void processRange(const String& range);


    // variables pertaining to current most HTTP request being serviced
    WiFiServer* server;
    FS*         gfs;

    WiFiClient 	client;
    String 		method;
    String 		uri;
    String 		contentLengthHeader;
    String 		depthHeader;
    String 		hostHeader;
    String		destinationHeader;
    String      overwrite;
    DepthType   depth;
    

    String 		_responseHeaders;
    bool		_chunked;
    int			_contentLength;
    int         _rangeStart;
    int         _rangeEnd;
};





