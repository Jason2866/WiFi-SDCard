#include "ESPWebDAV.h"

#define DEBUG_LEN 160

// Sections are copied from ESP8266Webserver

// ------------------------
String ESPWebDAV::getMimeType(const String& path)
{
    // ------------------------
    if (path.endsWith(".html")) return "text/html";
    else if (path.endsWith(".htm")) return "text/html";
    else if (path.endsWith(".css")) return "text/css";
    else if (path.endsWith(".txt")) return "text/plain";
    else if (path.endsWith(".js")) return "application/javascript";
    else if (path.endsWith(".json")) return "application/json";
    else if (path.endsWith(".png")) return "image/png";
    else if (path.endsWith(".gif")) return "image/gif";
    else if (path.endsWith(".jpg")) return "image/jpeg";
    else if (path.endsWith(".ico")) return "image/x-icon";
    else if (path.endsWith(".svg")) return "image/svg+xml";
    else if (path.endsWith(".ttf")) return "application/x-font-ttf";
    else if (path.endsWith(".otf")) return "application/x-font-opentype";
    else if (path.endsWith(".woff")) return "application/font-woff";
    else if (path.endsWith(".woff2")) return "application/font-woff2";
    else if (path.endsWith(".eot")) return "application/vnd.ms-fontobject";
    else if (path.endsWith(".sfnt")) return "application/font-sfnt";
    else if (path.endsWith(".xml")) return "text/xml";
    else if (path.endsWith(".pdf")) return "application/pdf";
    else if (path.endsWith(".zip")) return "application/zip";
    else if (path.endsWith(".gz")) return "application/x-gzip";
    else if (path.endsWith(".appcache")) return "text/cache-manifest";

    return "application/octet-stream";
}




// ------------------------
String ESPWebDAV::urlDecode(const String& text)
{
    // ------------------------
    String decoded = "";
    char temp[] = "0x00";
    unsigned int len = text.length();
    unsigned int i = 0;
    while (i < len)
    {
        char decodedChar;
        char encodedChar = text.charAt(i++);
        if ((encodedChar == '%') && (i + 1 < len))
        {
            temp[2] = text.charAt(i++);
            temp[3] = text.charAt(i++);
            decodedChar = strtol(temp, NULL, 16);
        }
        else
        {
            if (encodedChar == '+')
                decodedChar = ' ';
            else
                decodedChar = encodedChar;  // normal ascii char
        }
        decoded += decodedChar;
    }
    return decoded;
}





// ------------------------
String ESPWebDAV::urlToUri(const String& url)
{
    // ------------------------
    if (url.startsWith("http://"))
    {
        int uriStart = url.indexOf('/', 7);
        return url.substring(uriStart);
    }
    return url;
}



// ------------------------
bool ESPWebDAV::isClientWaiting()
{
    // ------------------------
    return server->hasClient();
}




// ------------------------
void ESPWebDAV::handleClient(const String& blank)
{
    // ------------------------
    processClient(&ESPWebDAV::handleRequest, blank);
}



// ------------------------
void ESPWebDAV::rejectClient(const String& rejectMessage)
{
    // ------------------------
    processClient(&ESPWebDAV::handleReject, rejectMessage);
}



// ------------------------
void ESPWebDAV::processClient(THandlerFunction handler, const String& message)
{
    // ------------------------
    // Check if a client has connected
    client = server->available();
    if (!client)
        return;

    // Wait until the client sends some data
    //XXXTIMEOUT !
    while (!client.available())
        delay(1);

    // reset all variables
    _chunked = false;
    _responseHeaders = String();
    _contentLength = CONTENT_LENGTH_NOT_SET;
    method.clear();
    uri.clear();
    contentLengthHeader.clear();
    depthHeader.clear();
    hostHeader.clear();
    destinationHeader.clear();
    overwrite.clear();

    // extract uri, headers etc
    if (parseRequest())
        // invoke the handler
        (this->*handler)(message);

    // finalize the response
    if (_chunked)
        sendContent("");

    // send all data before closing connection
    client.flush();
    // close the connection
    client.stop();
}


void ESPWebDAV::processRange(const String& range)
{
    // "Range": "bytes=0-5"

    size_t i = 0;
    while (i < range.length() && (range[i] < '0' || range[i] > '9'))
        i++;
    size_t j = i;
    while (j < range.length() && range[j] >= '0' && range[j] <= '9')
        j++;
    if (j > i)
    {
        _rangeStart = atoi(&range.c_str()[i]);
        _rangeEnd = atoi(&range.c_str()[j + 1]);
    }
    DBG_PRINTF("Range: %d -> %d\n", _rangeStart, _rangeEnd);
}


// ------------------------
bool ESPWebDAV::parseRequest()
{
    // ------------------------
    // Read the first line of HTTP request
    String req = client.readStringUntil('\r');
    client.readStringUntil('\n');

    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    int addr_start = req.indexOf(' ');
    int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1)
    {
        return false;
    }

    DBG_PRINTF("RECV >>>> --------------------\n");
    method = req.substring(0, addr_start);
    uri = urlDecode(req.substring(addr_start + 1, addr_end));
    DBG_PRINT("method: "); DBG_PRINT(method); DBG_PRINT(" url: "); DBG_PRINTLN(uri);

    // parse and finish all headers
    String headerName;
    String headerValue;
    _rangeStart = 0;
    _rangeEnd = -1;

    while (1)
    {
        req = client.readStringUntil('\r');
        client.readStringUntil('\n');
        if (req == "")
            // no more headers
            break;

        int headerDiv = req.indexOf(':');
        if (headerDiv == -1)
            break;

        headerName = req.substring(0, headerDiv);
        headerValue = req.substring(headerDiv + 2);
        DBG_PRINT("\t"); DBG_PRINT(headerName); DBG_PRINT(": "); DBG_PRINTLN(headerValue);

        if (headerName.equalsIgnoreCase("Host"))
            hostHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Depth"))
            depthHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Content-Length"))
            contentLengthHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Destination"))
            destinationHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Range"))
            processRange(headerValue);
        else if (headerName.equalsIgnoreCase("Overwrite"))
            overwrite = headerValue;
    }
    DBG_PRINTF("-------------------- <<<< RECV\n");

    return true;
}




// ------------------------
void ESPWebDAV::sendHeader(const String& name, const String& value, bool first)
{
    // ------------------------
    String headerLine = name + ": " + value + "\r\n";

    if (first)
        _responseHeaders = headerLine + _responseHeaders;
    else
        _responseHeaders += headerLine;
}



// ------------------------
void ESPWebDAV::send(const String& code, const char* content_type, const String& content)
{
    // ------------------------
    String header;
    _prepareHeader(header, code, content_type, content.length());

    client.write(header.c_str(), header.length());

    DBG_PRINTF("SENT >>>> ----------------------\n");
    DBG_PRINTF("---- header:\n%s", header.c_str());

    if (content.length())
    {
        sendContent(content);
        DBG_PRINTF("---- content (%d bytes):\n", (int)content.length());
        for (int i = 0; i < DEBUG_LEN && i < content.length(); i++)
            DBG_PRINTF("%c", content[i] < 32 || content[i] > 127 ? '.' : content[i]);
        if (content.length() > DEBUG_LEN) DBG_PRINTF("...");
        DBG_PRINTF("\n");
    }
    DBG_PRINTF("---------------------- <<<<< SENT\n");
}



// ------------------------
void ESPWebDAV::_prepareHeader(String& response, const String& code, const char* content_type, size_t contentLength)
{
    // ------------------------
    response = "HTTP/1.1 " + code + "\r\n";

    if (content_type)
        sendHeader("Content-Type", content_type, true);

    if ((size_t)_contentLength == CONTENT_LENGTH_NOT_SET)
        sendHeader("Content-Length", String(contentLength));
    else if ((size_t)_contentLength != CONTENT_LENGTH_UNKNOWN)
        sendHeader("Content-Length", String(_contentLength));
    else if ((size_t)_contentLength == CONTENT_LENGTH_UNKNOWN)
    {
        _chunked = true;
        //sendHeader("Accept-Ranges", "none");
        sendHeader("Transfer-Encoding", "chunked");
    }
    sendHeader("Connection", "close");

    response += _responseHeaders;
    response += "\r\n";
}



// ------------------------
void ESPWebDAV::sendContent(const String& content)
{
    // ------------------------
    const char * footer = "\r\n";
    size_t size = content.length();

    if (_chunked)
    {
        char chunkSize[32];
        snprintf(chunkSize, sizeof(chunkSize), "%x%s", size, footer);
        client.write(chunkSize, strlen(chunkSize));
    }

    DBG_PRINTF("---- %scontent (%d bytes):\n", _chunked ? "chunked " : "", (int)content.length());
    for (int i = 0; i < DEBUG_LEN && i < content.length(); i++)
        DBG_PRINTF("%c", content[i] < 32 || content[i] > 127 ? '.' : content[i]);
    if (content.length() > DEBUG_LEN) DBG_PRINTF("...");
    DBG_PRINTF("\n");

    client.write(content.c_str(), size);

    if (_chunked)
    {
        client.write(footer, 2);
        if (size == 0)
        {
            _chunked = false;
        }
    }
}



// ------------------------
void ESPWebDAV::sendContent_P(PGM_P content)
{
    // ------------------------
    const char * footer = "\r\n";
    size_t size = strlen_P(content);

    if (_chunked)
    {
        char chunkSize[32];
        snprintf(chunkSize, sizeof(chunkSize), "%x%s", size, footer);
        client.write(chunkSize, strlen(chunkSize));
    }

    client.write_P(content, size);

    if (_chunked)
    {
        client.write(footer, 2);
        if (size == 0)
        {
            _chunked = false;
        }
    }
}



// ------------------------
void ESPWebDAV::setContentLength(size_t len)
{
    // ------------------------
    _contentLength = len;
}


// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t size)
{
    // ------------------------
    size_t where = 0;

    while (where < size)
    {
        int timeout_ms = HTTP_MAX_POST_WAIT;
        while (!client.available() && client.connected() && timeout_ms--)
            delay(1);

        if (!client.available())
            break;

        where += client.read(buf + where, size - where);
    }

    return where;
}
