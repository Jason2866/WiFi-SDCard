/*  Using the WebDAV server
	From windows -
		Run: \\HOSTNAME\DavWWWRoot
		or Map Network Drive -> Connect to a Website
*/

#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ESPWebDAV.h>

#define HOSTNAME	"ESPWebDAV"
#define SD_CS		15

#ifndef STASSID
#define STASSID "ssid"
#define STAPSK "psk"
#endif

//FS& gfs = SPIFFS;
FS& gfs = LittleFS;
//FS& gfs = SDFS;

//WiFiServerSecure tcp(443);
WiFiServer tcp(80);

ESPWebDAV dav;


String statusMessage;
bool initFailed = false;


// ------------------------
void setup()
{
    // ------------------------
    WiFi.persistent(false);
    WiFi.hostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    Serial.begin(115200);
    WiFi.begin(STASSID, STAPSK);
    Serial.println("Connecting to " STASSID " ...");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to "); Serial.println(STASSID);
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
    Serial.print("Mode: "); Serial.println(WiFi.getPhyMode());

    gfs.begin();
    tcp.begin();
    dav.begin(&tcp, &gfs);

    Serial.println("WebDAV server started");
}


int listDir(const String& indent, const String& path)
{
    int dirCount = 0;
    Dir dir = gfs.openDir(path);
    while (dir.next())
    {
        ++dirCount;
        if (dir.isDirectory())
        {
            Serial.printf_P(PSTR("%s%s [Dir]\n"), indent.c_str(), dir.fileName().c_str());
            dirCount += listDir(indent + "  ", path + dir.fileName() + "/");
        }
        else
            Serial.printf_P(PSTR("%s%-16s (%u Bytes)\n"), indent.c_str(), dir.fileName().c_str(), (uint32_t)dir.fileSize());
    }
    return dirCount;
}

void help()
{
    Serial.printf("interactive: F/ormat D/ir\n");
}

// ------------------------
void loop()
{
    int c = Serial.read();
    if (c == 'F')
    {
        Serial.println("formatting...");
        if (gfs.format())
            Serial.println("Success");
        else
            Serial.println("Failure");
        auto f = gfs.open("readme.md", "w");
        f.printf("hello\n");
        f.close();
    }
    else if (c == 'D')
    {
        Serial.printf("-> %d dir/files\n", listDir("", "/"));
    }
    else if (c > 0)
        help();

    // ------------------------
    if (dav.isClientWaiting())
    {
        if (initFailed)
            return dav.rejectClient(statusMessage);

        // call handle if server was initialized properly
        dav.handleClient();
    }
}


