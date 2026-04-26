#include "notification.h"

#include <WiFi.h>
#include <HTTPClient.h>

static const char* PUSHOVER_URL = "https://api.pushover.net/1/messages.json";
static const char* PUSHOVER_TOKEN = "ai1hswwhdoksu53sf1a7nvnkg2efm9";
static const char* PUSHOVER_USER = "unvav3dcftou38k6okykqx8gqwep7x";
static const char* PUSHOVER_TITLE = "WasserKontrolle Alarm";
static const char* PUSHOVER_SOUND = "siren";
static const bool PUSHOVER_ENABLED = true;

static String urlEncode(const String& text) {
  String encoded;
  encoded.reserve(text.length() * 3);

  for (size_t i = 0; i < text.length(); i++) {
    const char c = text[i];

    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      encoded += buf;
    }
  }

  return encoded;
}

void sendPushover(String message) {
  if (!PUSHOVER_ENABLED) return;
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  if (!http.begin(PUSHOVER_URL)) return;

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "token=" + urlEncode(String(PUSHOVER_TOKEN)) +
                "&user=" + urlEncode(String(PUSHOVER_USER)) +
                "&title=" + urlEncode(String(PUSHOVER_TITLE)) +
                "&message=" + urlEncode(message) +
                "&sound=" + urlEncode(String(PUSHOVER_SOUND)) +
                "&priority=2"
                "&retry=60"
                "&expire=3600";

  http.POST(body);
  http.end();
}
