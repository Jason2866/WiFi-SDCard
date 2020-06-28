
// LOCK support is not robust with davfs2
// LOCK support is not mandatory
#define LOCK_SUPPORT 0

#include <ESP8266WiFi.h>
#include <FS.h>
#include <Hash.h>
#include <time.h>
#include <StreamString.h>

#include "ESPWebDAV.h"

// define cal constants
const char *months[]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *wdays[]  = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

#if LOCK_SUPPORT
#define SLOCK "LOCK"
#define SCLOCK ",LOCK"
#define SUNLOCK "UNLOCK"
#define SCUNLOCK ",UNLOCK"
#else
#define SLOCK ""
#define SCLOCK ""
#define SUNLOCK ""
#define SCUNLOCK ""
#endif

// ------------------------
void ESPWebDAV::begin(WiFiServer* server, FS* gfs)
// ------------------------
{
    this->server = server;
    this->gfs = gfs;
}

// ------------------------
void ESPWebDAV::handleNotFound()
{
    // ------------------------
    String message = "Not found\n";
    message += "URI: ";
    message += uri;
    message += " Method: ";
    message += method;
    message += "\n";

    sendHeader("Allow", "OPTIONS,MKCOL,POST,PUT");
    send("404 Not Found", "text/plain", message);
    DBG_PRINTLN("404 Not Found");
}



// ------------------------
void ESPWebDAV::handleReject(const String& rejectMessage)
{
    // ------------------------
    DBG_PRINT("Rejecting request: "); DBG_PRINTLN(rejectMessage);

    // handle options
    if (method.equals("OPTIONS"))
        return handleOptions(RESOURCE_NONE);

    // handle properties
    if (method.equals("PROPFIND"))
    {
        sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");
        setContentLength(CONTENT_LENGTH_UNKNOWN);
        send("207 Multi-Status", "application/xml;charset=utf-8", "");
        sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:multistatus xmlns:D=\"DAV:\"><D:response><D:href>/</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>Fri, 30 Nov 1979 00:00:00 GMT</D:getlastmodified><D:getetag>\"3333333333333333333333333333333333333333\"</D:getetag><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"));

        if (depthHeader.equals("1"))
        {
            sendContent(F("<D:response><D:href>/"));
            sendContent(rejectMessage);
            sendContent(F("</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>Fri, 01 Apr 2016 16:07:40 GMT</D:getlastmodified><D:getetag>\"2222222222222222222222222222222222222222\"</D:getetag><D:resourcetype/><D:getcontentlength>0</D:getcontentlength><D:getcontenttype>application/octet-stream</D:getcontenttype></D:prop></D:propstat></D:response>"));
        }

        sendContent(F("</D:multistatus>"));
        return;
    }
    else
        // if reached here, means its a 404
        handleNotFound();
}




// set http_proxy=http://localhost:36036
// curl -v -X PROPFIND -H "Depth: 1" http://Rigidbot/Old/PipeClip.gcode
// Test PUT a file: curl -v -T c.txt -H "Expect:" http://Rigidbot/c.txt
// C:\Users\gsbal>curl -v -X LOCK http://Rigidbot/EMA_CPP_TRCC_Tutorial/Consumer.cpp -d "<?xml version=\"1.0\" encoding=\"utf-8\" ?><D:lockinfo xmlns:D=\"DAV:\"><D:lockscope><D:exclusive/></D:lockscope><D:locktype><D:write/></D:locktype><D:owner><D:href>CARBON2\gsbal</D:href></D:owner></D:lockinfo>"
// ------------------------
void ESPWebDAV::handleRequest(const String& blank)
{
    DBG_PRINTF("############################################\n");
    (void)blank;
    // ------------------------
    ResourceType resource = RESOURCE_NONE;

    // does uri refer to a file or directory or a null?
    File tFile = gfs->open(uri, "r");
    if (tFile)
    {
        resource = tFile.isDirectory() ? RESOURCE_DIR : RESOURCE_FILE;
        tFile.close();
    }

    DBG_PRINT("\r\nm: "); DBG_PRINT(method);
    DBG_PRINT(" r: "); DBG_PRINT(resource);
    DBG_PRINT(" u: "); DBG_PRINTLN(uri);

    // add header that gets sent everytime
#if LOCK_SUPPORT
    sendHeader("DAV", "1, 2");
#else
    sendHeader("DAV", "1");
#endif
    sendHeader("Accept-Ranges", "bytes");

    // handle properties
    if (method.equals("PROPFIND"))
        return handleProp(resource);

    if (method.equals("GET"))
        return handleGet(resource, true);

    if (method.equals("HEAD"))
        return handleGet(resource, false);

    // handle options
    if (method.equals("OPTIONS"))
        return handleOptions(resource);

    // handle file create/uploads
    if (method.equals("PUT"))
        return handlePut(resource);

#if LOCK_SUPPORT
    // handle file locks
    if (method.equals("LOCK"))
        return handleLock(resource);

    if (method.equals("UNLOCK"))
        return handleUnlock(resource);
#endif

    if (method.equals("PROPPATCH"))
        return handlePropPatch(resource);

    // directory creation
    if (method.equals("MKCOL"))
        return handleDirectoryCreate(resource);

    // move a file or directory
    if (method.equals("MOVE"))
        return handleMove(resource);

    // delete a file or directory
    if (method.equals("DELETE"))
        return handleDelete(resource);

    // if reached here, means its a 404
    handleNotFound();
}



// ------------------------
void ESPWebDAV::handleOptions(ResourceType resource)
{
    (void)resource;
    // ------------------------
    DBG_PRINTLN("Processing OPTION");
    sendHeader("Allow", "PROPFIND,GET,DELETE,PUT,COPY,MOVE");
    send("200 OK", NULL, "");
}


#if LOCK_SUPPORT
// ------------------------
void ESPWebDAV::handleLock(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing LOCK");

    // does URI refer to an existing resource
    DBG_PRINTF("r=%d/%d\n", resource, RESOURCE_NONE);
    if (resource == RESOURCE_NONE)
        return handleNotFound();

    sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE" SCUNLOCK ",COPY" SCLOCK ",MOVE,HEAD,POST,PUT,GET");
    sendHeader("Lock-Token", "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");

    StreamString inXML;
    inXML.reserve((size_t)contentLengthHeader.toInt());
    size_t numRead = readBytesWithTimeout((uint8_t*)&inXML[0], (size_t)contentLengthHeader.toInt());
    if (numRead == 0)
        return handleNotFound();

    int startIdx = inXML.indexOf("<D:href>");
    int endIdx = inXML.indexOf("</D:href>");
    if (startIdx < 0 || endIdx < 0)
        return handleNotFound();

    String resp;
    resp.reserve(300 + uri.length() + 100 + (endIdx - startIdx) + 100);
    resp += F("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:prop xmlns:D=\"DAV:\"><D:lockdiscovery><D:activelock><D:locktype><write/></D:locktype><D:lockscope><exclusive/></D:lockscope><D:locktoken><D:href>urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9</D:href></D:locktoken><D:lockroot><D:href>");
    resp += uri;
    resp += F("</D:href></D:lockroot><D:depth>infinity</D:depth><D:owner><a:href xmlns:a=\"DAV:\">");
    resp += inXML.substring(startIdx + 8, endIdx);
    resp += F("</a:href></D:owner><D:timeout>Second-3600</D:timeout></D:activelock></D:lockdiscovery></D:prop>");

    send("200 OK", "application/xml;charset=utf-8", resp);
}



// ------------------------
void ESPWebDAV::handleUnlock(ResourceType resource)
{
    (void)resource;
    // ------------------------
    DBG_PRINTLN("Processing UNLOCK");
    sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,LOCK,MOVE,HEAD,POST,PUT,GET");
    sendHeader("Lock-Token", "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");
    send("204 No Content", NULL, "");
}

#endif // LOCK_SUPPORT

// ------------------------
void ESPWebDAV::handlePropPatch(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("PROPPATCH forwarding to PROPFIND");
    handleProp(resource);
}



// ------------------------
void ESPWebDAV::handleProp(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing PROPFIND");
    // check depth header
    DepthType depth = DEPTH_NONE;
    if (depthHeader.equals("1"))
        depth = DEPTH_CHILD;
    else if (depthHeader.equals("infinity"))
        depth = DEPTH_ALL;

    DBG_PRINT("Depth: "); DBG_PRINTLN(depth);

    // does URI refer to an existing resource
    if (resource == RESOURCE_NONE)
        return handleNotFound();

    if (resource == RESOURCE_FILE)
        sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
    else
        sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");

    setContentLength(CONTENT_LENGTH_UNKNOWN);
    send("207 Multi-Status", "application/xml;charset=utf-8", "");
    sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
    sendContent(F("<D:multistatus xmlns:D=\"DAV:\">"));

    // open this resource
    Dir entry = gfs->openDir(uri);
    sendPropResponse(false, entry);

    if ((resource == RESOURCE_DIR) && (depth == DEPTH_CHILD))
    {
        // append children information to message
        while (entry.next())
        {
            yield();
            sendPropResponse(true, entry);
        }
    }

    sendContent(F("</D:multistatus>"));
}



// ------------------------
void ESPWebDAV::sendPropResponse(boolean recursing, Dir& entry)
{
    // ------------------------
    // String fullResPath = "http://" + hostHeader + uri;
    String fullResPath = uri;

    if (recursing)
    {
        if (!fullResPath.endsWith("/"))
            fullResPath += '/';
        fullResPath += entry.fileName();
    }

    // get & convert time to required format
    // Tue, 13 Oct 2015 17:07:35 GMT
    time_t lastWrite = entry.fileTime();
    tm* gTm = gmtime(&lastWrite);
    String fileTimeStamp;
    {
        char buf[30];
        snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT", wdays[gTm->tm_wday], gTm->tm_mday, months[gTm->tm_mon], gTm->tm_year + 1900, gTm->tm_hour, gTm->tm_min, gTm->tm_sec);
        fileTimeStamp += buf;
    }

    // send the XML information about thyself to client
    sendContent(F("<D:response><D:href>"));
    // append full file path
    sendContent(fullResPath);
    sendContent(F("</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>"));
    // append modified date
    sendContent(fileTimeStamp);
    sendContent(F("</D:getlastmodified><D:getetag>"));
    // append unique tag generated from full path
    sendContent("\"" + sha1(fullResPath + fileTimeStamp) + "\"");
    sendContent(F("</D:getetag>"));

    File x = gfs->open(fullResPath, "r");
    bool isdir = x.isDirectory();

    DBG_PRINTF("entry: '%s' dir: %d/%d date: '%s'\n", fullResPath.c_str(), entry.isDirectory(), isdir, fileTimeStamp.c_str());

#if 1
    if (isdir)
#else
    if (fullResPath == "/" || // <- FS bug?
            entry.isDirectory())
#endif
    {
        sendContent(F("<D:resourcetype xmlns:D=\"DAV:\"><D:collection/></D:resourcetype>"));
    }
    else
    {
        sendContent(F("<D:resourcetype/><D:getcontentlength>"));
        // append the file size
        //sendContent(String(entry.fileSize()));
        sendContent(String(x.size()));
        sendContent(F("</D:getcontentlength><D:getcontenttype>"));
        // append correct file mime type
        sendContent(getMimeType(fullResPath));
        sendContent(F("</D:getcontenttype>"));
    }
    sendContent(F("</D:prop></D:propstat></D:response>"));

    x.close();
}




// ------------------------
void ESPWebDAV::handleGet(ResourceType resource, bool isGet)
{
    // ------------------------
    DBG_PRINTF("Processing GET (ressource=%d)\n", (int)resource);

    // does URI refer to an existing file resource
    if (resource != RESOURCE_FILE)
        return handleNotFound();

    long tStart = millis();
    File file = gfs->open(uri, "r");

    sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
    size_t fileSize = file.size();
    String contentType = getMimeType(uri);
    if (uri.endsWith(".gz") && contentType != "application/x-gzip" && contentType != "application/octet-stream")
        sendHeader("Content-Encoding", "gzip");

    if (!fileSize)
    {
        setContentLength(0);
        send("200 OK", contentType.c_str(), "");
        DBG_PRINTF("send empty file\n");
        return;
    }

    char buf[128]; /// XXX use stream:to(): file.to(client);

    // Content-Range: bytes 0-1023/146515
    // Content-Length: 1024

    int remaining;
    if (_rangeEnd == -1)
    {
        _rangeEnd = fileSize - 1;
        remaining = fileSize;
        setContentLength(remaining);
        send("200 OK", contentType.c_str(), "");
    }
    else
    {
        snprintf(buf, sizeof(buf), "bytes %d-%d/%d", _rangeStart, _rangeEnd, (int)fileSize);
        sendHeader("Content-Range", buf);
        remaining = _rangeEnd - _rangeStart + 1;
        setContentLength(remaining);
        send("206 Partial Content", contentType.c_str(), "");
    }

    if (isGet && file.seek(_rangeStart, SeekSet))
    {
        DBG_PRINTF("GET:");
        while (remaining > 0 && file.available())
        {
            // SD read speed ~ 17sec for 4.5MB file
            size_t toRead = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            size_t numRead = file.read((uint8_t*)buf, toRead);
            DBG_PRINTF("read %d bytes from file\n", (int)numRead);
            if (client.write(buf, numRead) != numRead)
            {
                DBG_PRINTF("file->net short transfer");
                ///XXXX transmit error ?
                //return handleWriteRead("Unable to send file content", &file);
                break;
            }

            for (int i = 0; i < 80 && i < numRead; i++)
                DBG_PRINTF("%c", buf[i] < 32 || buf[i] > 127 ? '.' : buf[i]);

            remaining -= numRead;
            DBG_PRINTF("wrote %d bytes to http client\n", (int)numRead);
        }
    }

    DBG_PRINT("File "); DBG_PRINT(fileSize); DBG_PRINT(" bytes sent in: "); DBG_PRINT((millis() - tStart) / 1000); DBG_PRINTLN(" sec");
}




// ------------------------
void ESPWebDAV::handlePut(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing Put");

    // does URI refer to a directory
    if (resource == RESOURCE_DIR)
        return handleNotFound();

    sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");

    File file;
    DBG_PRINTF("create file '%s'\n", uri.c_str());
    if (!(file = gfs->open(uri, "w")))
        return handleWriteError("Unable to create a new file", file);

    // file is created/open for writing at this point
    // did server send any data in put
    size_t contentLen = contentLengthHeader.toInt();
    DBG_PRINT(uri); DBG_PRINTF(" - ready for data (%i bytes)\n", (int)contentLen);

    if (contentLen != 0)
    {
        uint8_t buf[128];
        long tStart = millis();
        size_t numRemaining = contentLen;

        // read data from stream and write to the file
        while (numRemaining > 0)
        {
            size_t numToRead = numRemaining;
            if (numToRead > sizeof(buf))
                numToRead = sizeof(buf);
            auto numRead = readBytesWithTimeout(buf, numToRead);
            if (numRead == 0)
                break;

            size_t written = 0;
            while (written < numRead)
            {
                auto numWrite = file.write(buf + written, numRead - written);
                if (numWrite == 0 || (int)numWrite == -1)
                {
                    DBG_PRINTF("error: numread=%d write=%d written=%d\n", (int)numRead, (int)numWrite, (int)written);
                    return handleWriteError("Write data failed", file);
                }
                written += numWrite;
            }

            // reduce the number outstanding
            numRemaining -= numRead;
        }

        // detect timeout condition
        if (numRemaining)
            return handleWriteError("Timed out waiting for data", file);

        DBG_PRINT("File "); DBG_PRINT(contentLen - numRemaining); DBG_PRINT(" bytes stored in: "); DBG_PRINT((millis() - tStart) / 1000); DBG_PRINTLN(" sec");
    }

    DBG_PRINTF("file written ('%s': %d = %d bytes)\n", file.name(), (int)contentLen, (int)file.size());

    if (resource == RESOURCE_NONE)
        send("201 Created", NULL, "");
    else
        send("200 OK", NULL, "");
}


// ------------------------
void ESPWebDAV::handleWriteError(const String& message, File& file)
{
    // ------------------------
    // close this file
    file.close();
    // delete the wrile being written
    gfs->remove(uri);
    // send error
    send("500 Internal Server Error", "text/plain", message);
    DBG_PRINTLN(message);
}


// ------------------------
void ESPWebDAV::handleDirectoryCreate(ResourceType resource)
{
    // ------------------------
    DBG_PRINTF("Processing MKCOL (r=%d uri='%s')\n", (int)resource, uri.c_str());

    // does URI refer to anything
    if (resource != RESOURCE_NONE)
        return handleNotFound();

    while (uri.length() && uri[uri.length() - 1] == '/')
        uri.remove(uri.length() - 1);

    // create directory
    if (!gfs->mkdir(uri.c_str()))
    {
        // send error
        send("500 Internal Server Error", "text/plain", "Unable to create directory");
        DBG_PRINTLN("Unable to create directory");
        return;
    }

    DBG_PRINT(uri);	DBG_PRINTLN(" directory created");
    sendHeader("Allow", "OPTIONS,MKCOL" SCLOCK ",POST,PUT");
    send("201 Created", NULL, "");
}



// ------------------------
void ESPWebDAV::handleMove(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing MOVE");

    // does URI refer to anything
    if (resource == RESOURCE_NONE)
        return handleNotFound();

    if (destinationHeader.length() == 0)
        return handleNotFound();

    String dest = urlToUri(destinationHeader);

    DBG_PRINT("Move destination: "); DBG_PRINTLN(dest);

    // move file or directory
    if (!gfs->rename(uri, dest))
    {
        // send error
        send("500 Internal Server Error", "text/plain", "Unable to move");
        DBG_PRINTLN("Unable to move file/directory");
        return;
    }

    DBG_PRINTLN("Move successful");
    sendHeader("Allow", "OPTIONS,MKCOL" SCLOCK ",POST,PUT");
    send("201 Created", NULL, "");
}




// ------------------------
void ESPWebDAV::handleDelete(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing DELETE");

    // does URI refer to anything
    if (resource == RESOURCE_NONE)
        return handleNotFound();

    bool retVal;

    if (resource == RESOURCE_FILE)
        // delete a file
        retVal = gfs->remove(uri);
    else
        // delete a directory
        retVal = gfs->rmdir(uri);

    if (!retVal)
    {
        // send error
        send("500 Internal Server Error", "text/plain", "Unable to delete");
        DBG_PRINTLN("Unable to delete file/directory");
        return;
    }

    DBG_PRINTLN("Delete successful");
    sendHeader("Allow", "OPTIONS,MKCOL" SCLOCK ",POST,PUT");
    send("200 OK", NULL, "");
}

