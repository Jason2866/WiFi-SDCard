/*
  ESP8266WebServer.h - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  Simplified/Adapted for ESPWebDav:
  Copyright (c) 2018 Gurpreet Bal https://github.com/ardyesp/ESPWebDAV
  Copyright (c) 2020 David Gauchard https://github.com/d-a-v/ESPWebDAV

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/

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
void ESPWebDAV::handleClient()
{
    // ------------------------
    processClient(&ESPWebDAV::handleRequest, emptyString);
}

// ------------------------
void ESPWebDAV::processClient(THandlerFunction handler, const String& message)
{
    if (server->hasClient())
    {
        if (!client || !client.available())
        {
            // no or sleeping current client
            // take it over
            client = server->available();
            m_persistent_timer_ms = millis();
            DBG_PRINTF("NEW CLIENT-------------------------------------------------------\n");
        }
    }

    if (!client || !client.available())
        return;

    // no new client is waiting, allow more time to current client
    m_persistent_timer_ms = millis();

    m_persistent = ((millis() - m_persistent_timer_ms) < m_persistent_timer_init_ms);

    // reset all variables
    _chunked = false;
    _responseHeaders.clear();
    _contentLengthAnswer = (int)CONTENT_LENGTH_NOT_SET;
    method.clear();
    uri.clear();
    contentLengthHeader = 0;
    depthHeader.clear();
    hostHeader.clear();
    destinationHeader.clear();
    overwrite.clear();
    ifHeader.clear();
    lockTokenHeader.clear();

    // extract uri, headers etc
    if (parseRequest())
    {
        // invoke the handler
        (this->*handler)(message);
    }

    // finalize the response
    if (_chunked)
        sendContent("");

    if (!m_persistent)
        // close the connection
        client.stop();
}


void ESPWebDAV::processRange(const String& range)
{
    // "Range": "bytes=0-5"
    // "Range": "bytes=0-"

    size_t i = 0;
    while (i < range.length() && (range[i] < '0' || range[i] > '9'))
        i++;
    size_t j = i;
    while (j < range.length() && range[j] >= '0' && range[j] <= '9')
        j++;
    if (j > i)
    {
        _rangeStart = atoi(&range.c_str()[i]);
        if (range.c_str()[j + 1])
            _rangeEnd = atoi(&range.c_str()[j + 1]);
        else
            _rangeEnd = -1;
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

    DBG_PRINTF("############################################\n");
    DBG_PRINTF(">>>>>>>>>> RECV\n");
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
            contentLengthHeader = headerValue.toInt();
        else if (headerName.equalsIgnoreCase("Destination"))
            destinationHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Range"))
            processRange(headerValue);
        else if (headerName.equalsIgnoreCase("Overwrite"))
            overwrite = headerValue;
        else if (headerName.equalsIgnoreCase("If"))
            ifHeader = headerValue;
        else if (headerName.equalsIgnoreCase("Lock-Token"))
            lockTokenHeader = headerValue;
    }
    DBG_PRINTF("<<<<<<<<<< RECV\n");

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

    DBG_PRINTF(">>>>>>>>>> SENT\n");
    DBG_PRINTF("---- header:\n%s", header.c_str());

    if (content.length())
    {
        sendContent(content);
#if DBG_WEBDAV
        DBG_PRINTF("---- content (%d bytes):\n", (int)content.length());
        for (size_t i = 0; i < DEBUG_LEN && i < content.length(); i++)
            DBG_PRINTF("%c", content[i] < 32 || content[i] > 127 ? '.' : content[i]);
        if (content.length() > DEBUG_LEN) DBG_PRINTF("...");
        DBG_PRINTF("\n");
#endif
    }
    DBG_PRINTF("<<<<<<<<<< SENT\n");
}



// ------------------------
void ESPWebDAV::_prepareHeader(String& response, const String& code, const char* content_type, size_t contentLength)
{
    // ------------------------
    response = "HTTP/1.1 " + code + "\r\n";

    if (content_type)
        sendHeader("Content-Type", content_type, true);

    if ((size_t)_contentLengthAnswer == CONTENT_LENGTH_NOT_SET)
        sendHeader("Content-Length", String(contentLength));
    else if ((size_t)_contentLengthAnswer != CONTENT_LENGTH_UNKNOWN)
        sendHeader("Content-Length", String(_contentLengthAnswer));
    else //if ((size_t)_contentLengthAnswer == CONTENT_LENGTH_UNKNOWN)
    {
        _chunked = true;
        //sendHeader("Accept-Ranges", "none");
        sendHeader("Transfer-Encoding", "chunked");
    }
    if (m_persistent)
        sendHeader("Connection", "keep-alive");
    else
        sendHeader("Connection", "close");

    response += _responseHeaders;
    response += "\r\n";
}



// ------------------------
bool ESPWebDAV::sendContent(const String& content)
{
    return sendContent(content.c_str(), content.length());
}

bool ESPWebDAV::sendContent(const char* data, size_t size)
{
    // ------------------------
    if (_chunked)
    {
        char chunkSize[32];
        snprintf(chunkSize, sizeof(chunkSize), "%x\r\n", (int)size);
        size_t l = strlen(chunkSize);
        if (client.write(chunkSize, l) != l)
            return false;
        DBG_PRINTF("---- chunk %s\n", chunkSize);

        //XXXFIXME client.printf("%x...
    }

    DBG_PRINTF("---- %scontent (%d bytes):\n", _chunked ? "chunked " : "", (int)size);
    for (size_t i = 0; i < DEBUG_LEN && i < size; i++)
        DBG_PRINTF("%c", data[i] < 32 || data[i] > 127 ? '.' : data[i]);
    if (size > DEBUG_LEN) DBG_PRINTF("...");
    DBG_PRINTF("\n");

    if (client.write(data, size) != size)
        return false;

    if (_chunked)
    {
        if (client.write("\r\n", 2) != 2)
            return false;
        if (size == 0)
        {
            _chunked = false;
        }
    }

    return true;
}



// ------------------------
bool  ESPWebDAV::sendContent_P(PGM_P content)
{
    // ------------------------
    const char * footer = "\r\n";
    size_t size = strlen_P(content);

    if (_chunked)
    {
        char chunkSize[32];
        snprintf(chunkSize, sizeof(chunkSize), "%x%s", (int)size, footer);
        size_t l = strlen(chunkSize);
        if (client.write(chunkSize, l) != l)
            return false;
    }

    if (client.write_P(content, size) != size)
        return false;

    if (_chunked)
    {
        if (client.write(footer, 2) != 2)
            return false;
        if (size == 0)
        {
            _chunked = false;
        }
    }

    return true;
}



// ------------------------
void ESPWebDAV::setContentLength(size_t len)
{
    // ------------------------
    _contentLengthAnswer = len;
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
