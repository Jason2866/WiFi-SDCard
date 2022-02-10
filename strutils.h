
#pragma once

#include <Arduino.h>

void stripSlashes(String& name);
String date2date(time_t date);
String enc2c(const String& encoded);
String c2enc(const String& decoded);
void replaceFront (String& str, const String& from, const String& to);
String urlToUri(const String& url);
