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

#include <ESP8266WiFi.h>
#include <FS.h>
#include <Hash.h>
#include <time.h>
#include <coredecls.h> // crc32()

#include <ESPWebDAV.h>

// define cal constants
const char *months[]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *wdays[]  = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

#if WEBDAV_LOCK_SUPPORT
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

#define PROC "proc"

void ESPWebDAV::stripSlashes(String& name)
{
    size_t i = 0;
    while (i < name.length())
        if (name[i] == '/' && name.length() > 1 && ((i == name.length() - 1) || name[i + 1] == '/'))
            name.remove(i, 1);
        else
            i++;
}

#if WEBDAV_LOCK_SUPPORT

void ESPWebDAV::makeToken (String& ret, uint32_t pash, uint32_t ownash)
{
    char lock_token[17];
    snprintf(lock_token, sizeof(lock_token), "%08x%08x", pash, ownash);
    ret = lock_token;
}

void ESPWebDAV::extractLockToken (const String& someHeader, const char* start, const char* end, uint32_t& pash, uint32_t& ownash)
{
    pash = 0;
    ownash = 0;

    DBG_PRINTF("extracting lockToken from '%s'\n", someHeader.c_str());
    // extract "... <:[lock >
    int startIdx = someHeader.indexOf(start);
    if (startIdx < 0)
        return;
    startIdx += strlen(start);
    int endIdx = someHeader.indexOf(end, startIdx);
    if (endIdx < 0)
        return;
    DBG_PRINTF("found in [%d..%d[ (%d)\n", startIdx, endIdx, endIdx - startIdx);
    int len = endIdx - startIdx;
    if (len < 1 || len > 16)
        return;
    char cp [len + 1];
    memcpy(cp, &(someHeader.c_str()[startIdx]), len);
    cp[len] = 0;
    DBG_PRINTF("IfToken: '%s'\n", cp);
    int ownIdx = std::max(len - 8, 0);
    ownash = strtol(&cp[ownIdx], nullptr, 16);
    cp[ownIdx] = 0;
    pash = strtol(cp, nullptr, 16);
    DBG_PRINTF("IfToken: '%s' (0x%08x / 0x%08x)\n", cp, pash, ownash);
}

#endif // WEBDAV_LOCK_SUPPORT

bool ESPWebDAV::allowed (const String& uri, uint32_t ownash)
{
#if WEBDAV_LOCK_SUPPORT > 1

    String test = uri;
    while (test.length())
    {
        stripSlashes(test);
        DBG_PRINTF("lock: testing '%s'\n", test.c_str());
        uint32_t hash = crc32(test.c_str(), test.length());
        const auto& lock = _locks.find(hash);
        if (lock != _locks.end())
        {
            DBG_PRINTF("lock: found lock, %sowner!\n", lock->second == ownash? "": "not");
            return lock->second == ownash;
        }
        int s = test.lastIndexOf('/');
        if (s < 0)
            break;
        test.remove(s);
    }
    DBG_PRINTF("lock: none found\n");
    return true;

#else

    (void)uri;
    (void)ownash;
    return true;

#endif
}

bool ESPWebDAV::allowed (const String& uri, const String& xml)
{
    uint32_t hpash, anyownash;
    if (ifHeader.length())
        extractLockToken(ifHeader, "(<", ">", hpash, anyownash);
    else
    {
        int startIdx = xml.indexOf("<owner>");
        int endIdx = xml.indexOf("</owner>");
        anyownash = startIdx > 0 && endIdx > 0? crc32(&(xml.c_str()[startIdx + 7]), endIdx - startIdx - 7): 0;
    }
    return allowed(uri, anyownash);
}

void ESPWebDAV::stripName (String& name)
{
    if (name.length() > (size_t)_maxPathLength)
    {
        int dot = name.lastIndexOf('.');
        int newDot = _maxPathLength - (name.length() - dot);
        if (dot <= 0 || newDot < 0)
            name.remove(_maxPathLength);
        else
            name.remove(newDot, dot - newDot);
    }
}

void ESPWebDAV::dir (const String& path, Print* out)
{
    dirAction(path, true, [out](int depth, const String& parent, Dir& entry)->bool
        {
            (void)parent;
            for (int i = 0; i < depth; i++)
                out->print("    ");
            out->printf("%s%s%s\n",
                entry.isDirectory()?"[":"",
                entry.fileName().c_str(),
                entry.isDirectory()?"/]":"");
            return true;
        }, /*false=subdir first*/false);
}

size_t ESPWebDAV::makeVirtual (virt_e v, String& internal)
{
    if (v == VIRT_PROC)
    {
        internal = ESP.getFullVersion();
        internal += '\n';
    }
    return internal.length();
}

ESPWebDAV::virt_e ESPWebDAV::isVirtual (const String& uri)
{
    const char* n = &(uri.c_str()[0]);
    while (*n && *n == '/')
        n++;
    if (strcmp(n, PROC) == 0)
        return VIRT_PROC;
    return VIRT_NONE;
}

void ESPWebDAV::getPayload (StreamString& payload)
{
    int hcl = contentLengthHeader.toInt();
    DBG_PRINTF("content length=%s\n", contentLengthHeader.c_str());
    if (hcl > 0)
    {
        payload.reserve(hcl);
        while (payload.length() < (size_t)_contentLength && client.available())
        {
            uint8_t buf[16];
            auto n = client.read(buf, std::min((size_t)client.available(), sizeof(buf)));
            if (n > 0)
                payload.write(buf, n);
        }
        DBG_PRINTF("CONTENT:\n");
        DBG_PRINT(payload);
        DBG_PRINTF("\n");
    }
}

bool ESPWebDAV::dirAction (const String& path,
                           bool recursive,
                           const std::function<bool(int depth, const String& parent, Dir& entry)>& cb,
                           bool callAfter,
                           int depth)
{
    //DBG_PRINTF("diraction: scanning dir '%s'\n", path.c_str());
    Dir entry = gfs->openDir(path);

    while (entry.next())
        if (!entry.isDirectory())
        {
            //DBG_PRINTF("diraction: %s/%s (%d B): ", path.c_str(), entry.fileName().c_str(), (int)entry.fileSize());
            if (cb(depth, path, entry))
            {
                //DBG_PRINTF("(file-OK)\n");
            }
            else
            {
                //DBG_PRINTF("(file-abort)\n");
                return false;
            }
        }

    if (recursive)
    {
        entry = gfs->openDir(path);
        while (entry.next())
            if (entry.isDirectory())
            {
                //DBG_PRINTF("diraction: -------- %s/%s/\n", path.c_str(), entry.fileName().c_str());
                if (   ( callAfter || cb(depth, path, entry))
                    && dirAction(path + '/' + entry.fileName(), recursive, cb, callAfter, depth + 1)
                    && (!callAfter || cb(depth, path, entry)))
                {
                    //DBG_PRINTF("(dir-OK)\n");
                }
                else
                {
                    //DBG_PRINTF("(dir-abort)\n");
                    return false;
                }
            }
    }

    return true;
}

// ------------------------
void ESPWebDAV::begin(WiFiServer* server, FS* gfs)
// ------------------------
{
    this->server = server;
    this->gfs = gfs;

    fs::FSInfo64 info;
    if (gfs->info64(info))
        _maxPathLength = info.maxPathLength;
    else
        _maxPathLength = 16;
}

// ------------------------
void ESPWebDAV::handleIssue(int code, const char* text)
{
    // ------------------------
    String message;
    message.reserve(strlen(text) + uri.length() + method.length() + 32);
    message += text;
    message += "\nURI: ";
    message += uri;
    message += " Method: ";
    message += method;
    message += "\n";

    String err;
    err.reserve(strlen(text) + 32);
    err += code;
    err += ' ';
    err += text;

    //sendHeader("Allow", "OPTIONS,MKCOL,POST,PUT");

    DBG_PRINTF("Issue:\ntext='%s'\n", text);
    DBG_PRINTF("message='%s'\n", message.c_str());
    DBG_PRINTF("err='%s'\n", err.c_str());

    send(err, "text/plain", message);
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
        sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:multistatus xmlns:D=\"DAV:\"><D:response><D:href>/</D:href><D:propstat><D:status>HTTP/1.0 200 OK</D:status><D:prop><D:getlastmodified>Fri, 30 Nov 1979 00:00:00 GMT</D:getlastmodified><D:getetag>\"3333333333333333333333333333333333333333\"</D:getetag><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"));

        if (depthHeader.equals("1"))
        {
            sendContent(F("<D:response><D:href>/"));
            sendContent(rejectMessage);
            sendContent(F("</D:href><D:propstat><D:status>HTTP/1.0 200 OK</D:status><D:prop><D:getlastmodified>Fri, 01 Apr 2016 16:07:40 GMT</D:getlastmodified><D:getetag>\"2222222222222222222222222222222222222222\"</D:getetag><D:resourcetype/><D:getcontentlength>0</D:getcontentlength><D:getcontenttype>application/octet-stream</D:getcontenttype></D:prop></D:propstat></D:response>"));
        }

        sendContent(F("</D:multistatus>"));
        return;
    }
    else
    {
        // if reached here, means its a 404
        handleIssue(404, "Not found");
    }
}


// ------------------------
void ESPWebDAV::handleRequest(const String& blank)
{
    (void)blank;

    stripSlashes(uri);

    // ------------------------
    ResourceType resource = RESOURCE_NONE;

    // check depth header
    depth = DEPTH_NONE;
    if (depthHeader.length())
    {
        if (depthHeader.equals("1"))
            depth = DEPTH_CHILD;
        else if (depthHeader.equals("infinity"))
            depth = DEPTH_ALL;
        DBG_PRINT("Depth: "); DBG_PRINTLN(depth);
    }

    // does uri refer to a file or directory or a null?
    File file = gfs->open(uri, "r");
    if (file)
    {
        resource = file.isDirectory() ? RESOURCE_DIR : RESOURCE_FILE;
        DBG_PRINTF("resource: '%s' is %s\n", uri.c_str(), resource == RESOURCE_DIR? "dir": "file");
    }
    else
        DBG_PRINTF("resource: '%s': no file nor dir\n", uri.c_str());


    DBG_PRINT("\r\nm: "); DBG_PRINT(method);
    DBG_PRINT(" r: "); DBG_PRINT(resource);
    DBG_PRINT(" u: "); DBG_PRINTLN(uri);

    // add header that gets sent everytime
#if WEBDAV_LOCK_SUPPORT
    sendHeader("DAV", "1, 2");
#else
    sendHeader("DAV", "1");
#endif
    sendHeader("Accept-Ranges", "bytes");

    // handle properties
    if (method.equals("PROPFIND"))
        return handleProp(resource, file);

    if (method.equals("GET"))
        return handleGet(resource, file, true);

    if (method.equals("HEAD"))
        return handleGet(resource, file, false);

    // handle options
    if (method.equals("OPTIONS"))
        return handleOptions(resource);

    // handle file create/uploads
    if (method.equals("PUT"))
        return handlePut(resource);

#if WEBDAV_LOCK_SUPPORT
    // handle file locks
    if (method.equals("LOCK"))
        return handleLock(resource);

    if (method.equals("UNLOCK"))
        return handleUnlock(resource);
#endif

    if (method.equals("PROPPATCH"))
        return handlePropPatch(resource, file);

    // directory creation
    if (method.equals("MKCOL"))
        return handleDirectoryCreate(resource);

    // move a file or directory
    if (method.equals("MOVE"))
        return handleMove(resource, file);

    // delete a file or directory
    if (method.equals("DELETE"))
        return handleDelete(resource);

    // delete a file or directory
    if (method.equals("COPY"))
        return handleCopy(resource, file);

    // if reached here, means its a 404
    handleIssue(404, "Not found");
}



// ------------------------
void ESPWebDAV::handleOptions(ResourceType resource)
{
    (void)resource;
    // ------------------------
    DBG_PRINTLN("Processing OPTION");

#if DBG_WEBDAV
    StreamString inXML;
    getPayload(inXML);
#endif

    sendHeader("Allow", "PROPFIND,GET,DELETE,PUT,COPY,MOVE");
    send("200 OK", NULL, "");
}


#if WEBDAV_LOCK_SUPPORT

// ------------------------
void ESPWebDAV::handleLock(ResourceType resource)
{
    // ------------------------
    DBG_PRINTLN("Processing LOCK");

    // does URI refer to an existing resource
    (void)resource;
    DBG_PRINTF("r=%d/%d\n", resource, RESOURCE_NONE);

    sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE" SCUNLOCK ",COPY" SCLOCK ",MOVE,HEAD,POST,PUT,GET");

    StreamString inXML;
    getPayload(inXML);

#if WEBDAV_LOCK_SUPPORT > 1
    // lock owner
    uint32_t hpash, ownash;
    if (ifHeader.length())
        extractLockToken(ifHeader, "(<", ">", hpash, ownash);
    else
    {
        int startIdx, endIdx;
        startIdx = inXML.indexOf("<owner>");
        endIdx = inXML.indexOf("</owner>");
        ownash = startIdx > 0 && endIdx > 0? crc32(&inXML[startIdx + 7], endIdx - startIdx - 7): 0;
    }

    DBG_PRINTF("asker: 0x%08x\n", ownash);
    stripSlashes(uri);
    uint32_t pash = crc32(uri.c_str(), uri.length());
    const auto& lock = _locks.find(pash);
    if (lock == _locks.end())
    {
        _locks[pash] = ownash;
    }
    else
    {
        if (lock->second != ownash)
        {
            DBG_PRINTF("cannot relock '%s' (owner is 0x%08x)\n", uri.c_str(), lock->second);
            return handleIssue(423, "Locked");
        }
        DBG_PRINTF("owner has relocked\n");
    }
#else
    const char* lock_token = "0";
#endif

    inXML.clear();
    String lock_token;
    makeToken(lock_token, pash, ownash);
    sendHeader("Lock-Token", lock_token);

#if 1
    String resp;
    resp.reserve(500 + uri.length());
    resp += F("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                  "<D:prop xmlns:D=\"DAV:\">"
                      "<D:lockdiscovery>"
                          "<D:activelock>"
                              "<D:locktoken>"
                                  "<D:href>");
    resp +=                           lock_token;
    resp += F(                    "</D:href>"
                              "</D:locktoken>"
#if 0
                              "<D:locktype>"
                                  "<write/>"
                              "</D:locktype>"
                              "<D:lockscope>"
                                  "<exclusive/>"
                              "</D:lockscope>"
                              "<D:lockroot>"
                                  "<D:href>");
    resp +=                           uri;
    resp += F(                    "</D:href>"
                              "</D:lockroot>"
                              "<D:depth>"
                                  "infinity"
                              "</D:depth>");
#if 0
    if (href.length())
    {
        resp += F(            "<D:owner>"
                                  "<a:href xmlns:a=\"DAV:\">");
        resp +=                       href;
        resp += F(                "</a:href>"
                              "</D:owner>");
    }
#endif
    resp += F(                "<D:timeout>"
                                  "Second-3600"
                              "</D:timeout>"
#endif
                          "</D:activelock>"
                      "</D:lockdiscovery>"
                  "</D:prop>");
    send("200 OK", "application/xml;charset=utf-8", resp);
#else
    send("200 OK", "application/xml;charset=utf-8", "");
#endif
}



// ------------------------
void ESPWebDAV::handleUnlock(ResourceType resource)
{
#if WEBDAV_LOCK_SUPPORT > 1
    stripSlashes(uri);
    uint32_t pash = crc32(uri.c_str(), uri.length());

    uint32_t hpash, hownash;
    extractLockToken(lockTokenHeader, "<", ">", hpash, hownash);

    auto lock = _locks.find(pash);
    if (lock == _locks.end())
    {
            DBG_PRINTF("wasn't locked: '%s'\n", uri.c_str());
            return handleIssue(423, "Locked");
    }
    if (lock->second != hownash)
    {
            DBG_PRINTF("lock found, bad owner 0x%08x != 0x%08x\n", hownash, lock->second);
            return handleIssue(423, "Locked");
    }
    _locks.erase(lock);
#endif

    (void)resource;
    // ------------------------
    DBG_PRINTLN("Processing UNLOCK");
    sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,LOCK,MOVE,HEAD,POST,PUT,GET");
    send("204 No Content", NULL, "");
}

#endif // WEBDAV_LOCK_SUPPORT

// ------------------------
void ESPWebDAV::handlePropPatch(ResourceType resource, File& file)
{
    // ------------------------
    DBG_PRINTLN("PROPPATCH forwarding to PROPFIND");
    handleProp(resource, file);
}



// ------------------------
void ESPWebDAV::handleProp(ResourceType resource, File& file)
{
    // ------------------------
    DBG_PRINTLN("Processing PROPFIND");
    auto v = isVirtual(uri);

    StreamString payload;
    getPayload(payload);
    if (v)
    {
        resource = RESOURCE_FILE;
    }
    // does URI refer to an existing resource
    else
        if (resource == RESOURCE_NONE)
            return handleIssue(404, "Not found");

    if (payload.indexOf("lockdiscovery") < 0 && !allowed(uri))
        return handleIssue(423, "Locked");

    if (resource == RESOURCE_FILE)
        sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
    else
        sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");

    setContentLength(CONTENT_LENGTH_UNKNOWN);
    send("207 Multi-Status", "application/xml;charset=utf-8", "");
    sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
    sendContent(F("<D:multistatus xmlns:D=\"DAV:\">"));

    if (v)
    {
        // virtual file
        sendPropResponse(false, uri.c_str(), 1024, time(nullptr));
    }
    else if (file.isFile() || depth == DEPTH_NONE)
    {
        DBG_PRINTF("----- PROP FILE '%s':\n", uri.c_str());
        sendPropResponse(file.isDirectory(), uri.c_str(), file.size(), file.getLastWrite());
    }
    else
    {
        DBG_PRINTF("----- PROP DIR '%s':\n", uri.c_str());
        ////XXX FIXME DEPTH=oo must walk the tree

        if (uri.length() == 0 || (uri.length() == 1 && uri[0] == '/'))
        {
            ///XXX fixme: more generic way to list virtual file list
            sendPropResponse(false, PROC, 1024, time(nullptr));
        }

        Dir entry = gfs->openDir(uri);
        while (entry.next())
        {
            yield();
            String name = entry.fileName();
            String path;
            path.reserve(uri.length() + 1 + name.length());
            path += uri;
            path += '/';
            path += name;
            stripSlashes(path);
            sendPropResponse(entry.isDirectory(), path.c_str(), entry.fileSize(), entry.fileTime());
        }
    }

    sendContent(F("</D:multistatus>"));
}

// ------------------------
void ESPWebDAV::sendProp1Response(const String& what, const String& response)
{
    String one;
    one.reserve(100 + 2*what.length() + response.length());
    one += F("<D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:");
    one += what;
    one += F(">");
    one += response;
    one += F("</D:");
    one += what;
    one += F("></D:prop></D:propstat>");
    sendContent(one);
}

// ------------------------
void ESPWebDAV::sendPropResponse(bool isDir, const String& fullResPath, size_t size, time_t lastWrite)
{
    // get & convert time to required format
    // Tue, 13 Oct 2015 17:07:35 GMT
    tm* gTm = gmtime(&lastWrite);
    String fileTimeStamp;
    {
        char buf[30];
        snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT", wdays[gTm->tm_wday], gTm->tm_mday, months[gTm->tm_mon], gTm->tm_year + 1900, gTm->tm_hour, gTm->tm_min, gTm->tm_sec);
        fileTimeStamp += buf;
    }

    String blah;
    blah.reserve(100);
    blah += F("<D:response><D:href>");
    blah += fullResPath;
    blah += F("</D:href>");
    sendContent(blah);

    sendProp1Response(F("getlastmodified"), fileTimeStamp.c_str());
    sendProp1Response(F("getetag"), sha1(fullResPath + fileTimeStamp)); // XXX <= use crc32 like the others

    DBG_PRINTF("-----\nentry: '%s'(dir:%d) date: '%s'\n-----\n",
        fullResPath.c_str(), isDir,
        fileTimeStamp.c_str());

    if (isDir)
    {
        sendProp1Response(F("resourcetype"), F("<D:collection/>"));

        fs::FSInfo64 info;
        if (gfs->info64(info))
        {
            sendProp1Response("quota-available-bytes", String(1.0 * info.totalBytes, 0));
            sendProp1Response("quota-used-bytes", String(1.0 * info.usedBytes, 0));
        }
    }
    else
    {
        sendProp1Response(F("getcontentlength"), String(size));
        sendProp1Response(F("getcontenttype"), getMimeType(fullResPath));
    }

    //sendProp1Response(F("displayname"), "yeah");

    sendContent(F("</D:response>"));
}


// ------------------------
void ESPWebDAV::handleGet(ResourceType resource, File& file, bool isGet)
{
    // ------------------------
    DBG_PRINTF("Processing GET (ressource=%d)\n", (int)resource);
    auto v = isVirtual(uri);

    // does URI refer to an existing file resource
    if (resource != RESOURCE_FILE && !v)
        return handleIssue(404, "Not found");

    // no lock on GET

#if DBG_WEBDAV
    long tStart = millis();
#endif

    sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
    size_t fileSize = file.size();
    String contentType = getMimeType(uri);
    if (uri.endsWith(".gz") && contentType != "application/x-gzip" && contentType != "application/octet-stream")
        sendHeader("Content-Encoding", "gzip");

    String internal = emptyString;
    if (v)
    {
        fileSize = makeVirtual(v, internal);
    }
    else if (!fileSize)
    {
        setContentLength(0);
        send("200 OK", contentType.c_str(), "");
        DBG_PRINTF("send empty file\n");
        return;
    }

    char buf[128]; /// XXX use stream:to(): file.to(client);

    // Content-Range: bytes 0-1023/146515
    // Content-Length: 1024

    constexpr bool chunked = false;

    int remaining;
    if (_rangeStart == 0 && (_rangeEnd < 0 || _rangeEnd == (int)fileSize - 1))
    {
        _rangeEnd = fileSize - 1;
        remaining = fileSize;
        setContentLength(chunked? CONTENT_LENGTH_UNKNOWN: remaining);
        send("200 OK", contentType.c_str(), "");
    }
    else
    {
        if (_rangeEnd == -1 || _rangeEnd >= (int)fileSize)
        {
            _rangeEnd = _rangeStart + (2*TCP_MSS - 100);
            if (_rangeEnd >= (int)fileSize)
                _rangeEnd = fileSize - 1;
        }
        snprintf(buf, sizeof(buf), "bytes %d-%d/%d", _rangeStart, _rangeEnd, (int)fileSize);
        sendHeader("Content-Range", buf);
        remaining = _rangeEnd - _rangeStart + 1;
        setContentLength(chunked? CONTENT_LENGTH_UNKNOWN: remaining);
        send("206 Partial Content", contentType.c_str(), "");
    }

    if (isGet && (internal.length() || file.seek(_rangeStart, SeekSet)))
    {
        DBG_PRINTF("GET: (%d bytes, chunked=%d, remain=%d)", remaining, chunked, remaining);
        
        if (internal.length())
        {
            printf("GLO %s %d %d\n", internal.c_str(), (int)_rangeStart, (int)remaining);
            if (   ( chunked && !sendContent(&internal.c_str()[_rangeStart], remaining))
                || (!chunked && client.write(&internal.c_str()[_rangeStart], remaining) != (size_t)remaining))
            {
                DBG_PRINTF("file->net short transfer");
            }
        }
        else
            while (remaining > 0 && file.available())
            {
                size_t toRead = (size_t)remaining > sizeof(buf) ? sizeof(buf) : remaining;
                size_t numRead = file.read((uint8_t*)buf, toRead);
                DBG_PRINTF("read %d bytes from file\n", (int)numRead);

                if (   ( chunked && !sendContent(buf, numRead))
                    || (!chunked && client.write(buf, numRead) != numRead))
                {
                    DBG_PRINTF("file->net short transfer");
                    ///XXXX transmit error ?
                    //return handleWriteRead("Unable to send file content", &file);
                    break;
                }

    #if DBG_WEBDAV
                for (size_t i = 0; i < 80 && i < numRead; i++)
                    DBG_PRINTF("%c", buf[i] < 32 || buf[i] > 127 ? '.' : buf[i]);
    #endif

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
        return handleIssue(404, "Not found");

    if (!allowed(uri))
        return handleIssue(423, "Locked");

    sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");

    File file;
    stripName(uri);
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
#if DBG_WEBDAV
        long tStart = millis();
#endif
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
    DBG_PRINTF("Processing MKCOL (r=%d uri='%s' cl=%s)\n", (int)resource, uri.c_str(), contentLengthHeader.c_str());

    if (contentLengthHeader.length() && contentLengthHeader.toInt() > 0)
        return handleIssue(415, "Unsupported Media Type");

    // does URI refer to anything
    if (resource != RESOURCE_NONE)
        return handleIssue(405, "Not allowed");

    stripSlashes(uri);
    int parentLastIndex = uri.lastIndexOf('/');
    if (parentLastIndex > 0)
    {
        File testParent = gfs->open(uri.substring(0, parentLastIndex), "r");
        if (!testParent.isDirectory())
            return handleIssue(409, "Conflict");
    }

    if (!gfs->mkdir(uri))
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
void ESPWebDAV::handleMove(ResourceType resource, File& src)
{
    const char* successCode = "201 Created";

    // ------------------------
    DBG_PRINTLN("Processing MOVE");
#if DBG_WEBDAV
    dir("/", &Serial);
#endif

    // does URI refer to anything
    if (   resource == RESOURCE_NONE
        || destinationHeader.length() == 0)
    {
        return handleIssue(404, "Not found");
    }

    String dest = urlToUri(destinationHeader);
    stripSlashes(dest);
    stripName(dest);
    DBG_PRINT("Move destination: "); DBG_PRINTLN(dest);

    if (!allowed(uri) || !allowed(dest))
        return handleIssue(423, "Locked");

    File destFile = gfs->open(dest, "r");
    if (destFile && !destFile.isFile())
    {
        dest += '/';
        dest += src.name();
        stripSlashes(dest);
        stripName(dest);
        destFile.close();
        destFile = gfs->open(dest, "r");
        successCode = "204 No Content"; // MOVE to existing collection resource didn't give 204
    }

    if (destFile)
    {
        if (overwrite.equalsIgnoreCase("F"))
            return handleIssue(412, "Precondition Failed");
        if (destFile.isDirectory())
        {
            destFile.close();
            deleteDir(dest);
        }
        else
        {
            destFile.close();
            gfs->remove(dest);
        }
    }

    src.close();

    DBG_PRINTF("finally rename '%s' -> '%s'\n", uri.c_str(), dest.c_str());

    if (!gfs->rename(uri, dest))
    {
        // send error
        send("500 Internal Server Error", "text/plain", "Unable to move");
        DBG_PRINTLN("Unable to move file/directory");
        return;
    }

#if DBG_WEBDAV
    dir("/", &Serial);
#endif

    DBG_PRINTLN("Move successful");
    sendHeader("Allow", "OPTIONS,MKCOL" SCLOCK ",POST,PUT");
    send(successCode, NULL, "");
}


// ------------------------
bool ESPWebDAV::deleteDir (const String& dir)
{
    dirAction(dir, true, [this](int depth, const String& parent, Dir& entry)->bool
        {
            (void)depth;
            String toRemove;
            toRemove.reserve(parent.length() + entry.fileName().length() + 2);
            toRemove += parent;
            toRemove += '/';
            toRemove += entry.fileName();
            bool ok = !!(entry.isDirectory()? gfs->rmdir(toRemove): gfs->remove(toRemove));
            DBG_PRINTF("DELETE %s %s: %s\n", entry.isDirectory()?"[ dir]":"[file]",toRemove.c_str(), ok? "ok":"bad");
            return ok;
        });

    gfs->rmdir(uri);
    // observation: with littleFS, when the last file of a directory is
    // removed, the parent directory is removed.
    // XXX or ensure it is not there anymore
    return true;
}

// ------------------------
void ESPWebDAV::handleDelete(ResourceType resource)
{
    // ------------------------
    DBG_PRINTF("Processing DELETE '%s'\n", uri.c_str());

    // does URI refer to anything
    if (resource == RESOURCE_NONE)
        return handleIssue(404, "Not found");

    if (!allowed(uri))
        return handleIssue(423, "Locked");

    bool retVal;
    if (resource == RESOURCE_FILE)
        // delete a file
        retVal = gfs->remove(uri);
    else
        retVal = deleteDir(uri);

    // for some reason, parent dir can be removed if empty
    // need to leave it there (also to pass compliance tests).
    int parentIdx = uri.lastIndexOf('/');
    uri.remove(parentIdx);
    gfs->mkdir(uri);

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


bool ESPWebDAV::copyFile (File srcFile, const String& destName)
{
    File dest;
    if (overwrite.equalsIgnoreCase("F"))
    {
        dest = gfs->open(destName, "r");
        if (dest)
        {
            DBG_PRINTF("copy dest '%s' already exists and overwrite is false\n", destName.c_str());
            handleIssue(412, "Precondition Failed");
            return false;
        }
    }
    dest = gfs->open(destName, "w");
    if (!dest)
    {
        handleIssue(413, "Request Entity Too Large");
        return false;
    }
    while (srcFile.available())
    {
        ///XXX USE STREAMTO
        yield();
        char cp[128];
        auto nb = srcFile.read((uint8_t*)cp, sizeof(cp));
        if (!nb)
        {
            DBG_PRINTF("copy: short read\n");
            handleIssue(500, "Internal Server Error");
            return false;
        }
        auto wr = dest.write(cp, nb);
        if (wr != nb)
        {
            DBG_PRINTF("copy: short write wr=%d != rd=%d\n", (int)wr, (int)nb);
            handleIssue(500, "Internal Server Error");
            return false;
        }
    }
    dest.close();
    return true;
}

// ------------------------
void ESPWebDAV::handleCopy(ResourceType resource, File& src)
{
    const char* successCode = "201 Created";

    // ------------------------
    DBG_PRINTLN("Processing COPY");

    if (resource == RESOURCE_NONE)
        return handleIssue(404, "Not found");

    if (!src) // || resource != RESOURCE_FILE)
        return handleIssue(413, "Request Entity Too Large");

    StreamString payload;
    getPayload(payload);

    String destParentPath = destinationHeader;
    {
        int j = -1;
        for (int i = 0; i < 3; i++)
            j = destParentPath.indexOf('/', j + 1);
        destParentPath.remove(0,j);
    }

    String destPath = destParentPath;
    if (destPath.length())
    {
        if (destPath[destPath.length() - 1] == '/')
        {
            // add file name
            destPath += src.name();
            successCode = "204 No Content"; // COPY to existing resource should give 204 (RFC2518:S8.8.5)
        }
        else
        {
            int lastSlash = destParentPath.lastIndexOf('/');
            if (lastSlash > 0)
                destParentPath.remove(lastSlash);
        }
    }

    DBG_PRINTF("copy: src='%s'=>'%s' dest='%s'=>'%s' parent:'%s'\n",
        uri.c_str(), src.fullName(),
        destinationHeader.c_str(), destPath.c_str(),
        destParentPath.c_str());
    File destParent = gfs->open(destParentPath, "r");

    stripName(destPath);
    if (/*!allowed(uri) ||*/ !allowed(destParentPath) || !allowed(destPath))
        return handleIssue(423, "Locked");

    // copy directory
    if (src.isDirectory())
    {
        DBG_PRINTF("Source is directory\n");
        if (destParent.isFile())
        {
            DBG_PRINTF("'%s' is not a directory\n", destParentPath.c_str());
            return handleIssue(409, "Conflict");
        }

        if (!dirAction(src.fullName(), depth == DEPTH_ALL, [this, destParentPath](int depth, const String& parent, Dir& source)->bool
            {
                (void)depth;
                (void)parent;
                String destNameX = destParentPath + '/' + source.fileName();
                stripName(destNameX);
                DBG_PRINTF("COPY: '%s' -> '%s'\n", source.fileName().c_str(), destNameX.c_str());
                return copyFile(gfs->open(source.fileName(), "r"), destNameX);
            }))
        {
            return handleIssue(402, "Payment Required");
        }
    }
    else
    {
        DBG_PRINTF("Source is file\n");

        // (COPY into non-existant collection '/litmus/nonesuch' succeeded)
        if (!destParent || !destParent.isDirectory())
        {
            DBG_PRINTF("dest dir '%s' not existing\n", destParentPath.c_str());
            return handleIssue(409, "Conflict");
        }

        // copy file

        if (!copyFile(src, destPath))
            return;
    }

    DBG_PRINTLN("COPY successful\n");
    sendHeader("Allow", "OPTIONS,MKCOL" SCLOCK ",POST,PUT");
    send(successCode, NULL, "");
}

