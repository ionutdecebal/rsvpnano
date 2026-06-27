#pragma once

#include <Arduino.h>

#include "text/TextNormalizer.h"

namespace RsvpText {

struct RsvpDirectiveValues {
  String title;
  String author;
};

String stripBom(String text);
bool prefixHasBoundary(const String &lowered, const char *prefix);
bool chapterTitleFromLine(const String &line, String &title);
String directiveValue(const String &line, const char *directive);
RsvpDirectiveValues readRsvpDirectiveValues(const String &path);
String readRsvpDirectiveValue(const String &path, const char *directive);

} // namespace RsvpText
