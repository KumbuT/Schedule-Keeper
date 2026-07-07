#pragma once
#include <ESPAsyncWebServer.h>

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
  String _buildStatusJson();
};