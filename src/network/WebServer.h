#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class AppWebServer {
public:
  static AppWebServer& instance();
  void begin();
  void broadcastStatus();  // Call from main loop every 1s

private:
  AppWebServer() : _server(80), _ws("/ws") {}
  AsyncWebServer _server;
  AsyncWebSocket _ws;

  void   _setupRoutes();
  void   _onWsEvent(AsyncWebSocket*, AsyncWebSocketClient*,
                    AwsEventType, void*, uint8_t*, size_t);
  const String &_buildStatusJson();

  // Reused across every 1 Hz status broadcast instead of constructing a fresh
  // JsonDocument each second. clear() keeps the already-grown internal pool, so
  // repeated broadcasts stop churning (and fragmenting) the heap. Only touched
  // from the single-threaded main loop, so no locking is needed.
  JsonDocument _statusDoc;
  // Reused serialized-JSON buffer (reserve()'d once) so the 1 Hz broadcast
  // doesn't heap-allocate a fresh String every second.
  String _statusJson;
};
