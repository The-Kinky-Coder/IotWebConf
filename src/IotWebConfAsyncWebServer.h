/**
 * IotWebConfAsyncWebServer.h -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef IotWebConfAsyncWebServer_h
#define IotWebConfAsyncWebServer_h

// Only compile this wrapper if ESPAsyncWebServer is available
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)

#include <Arduino.h>
#include <IotWebConfWebServerWrapper.h>
#include <ESPAsyncWebServer.h>

namespace iotwebconf
{

class IotWebConf;

/**
 * AsyncWebRequestWrapper - Wraps AsyncWebServerRequest for IotWebConf compatibility
 * 
 * This class adapts ESPAsyncWebServer's request handling to the WebRequestWrapper interface.
 * Key differences from synchronous WebServer:
 * 
 * 1. Request-based API: All operations are on the request object, not the server
 * 2. No built-in requestAuthentication(): We manually construct the 401 response
 * 3. Chunked responses: Uses AsyncWebServerResponse with chunked mode
 * 4. Connection lifetime: AsyncWebServer manages connection, stop() is handled internally
 */
class AsyncWebRequestWrapper : public WebRequestWrapper
{
public:
  /**
   * Constructor - wraps an AsyncWebServerRequest pointer
   * @param request - Pointer to the async request being handled
   */
  AsyncWebRequestWrapper(AsyncWebServerRequest* request) { this->_request = request; }

  /**
   * Get the Host header from the request
   */
  const String hostHeader() const override
  {
    if (this->_request->hasHeader("Host"))
    {
      return this->_request->header("Host");
    }
    return String("");
  }

  /**
   * Get the local IP address
   * Note: AsyncWebServer doesn't expose client() directly, so we use the server's local IP
   */
  IPAddress localIP() override
  {
    return this->_request->client()->localIP();
  }

  /**
   * Get the local port
   */
  uint16_t localPort() override
  {
    return this->_request->client()->localPort();
  }

  /**
   * Get the request URI
   */
  const String uri() const override
  {
    return this->_request->url();
  }

  /**
   * Authenticate using HTTP Basic Auth
   * @param username - Expected username
   * @param password - Expected password
   * @return true if authentication successful
   */
  bool authenticate(const char* username, const char* password) override
  {
    return this->_request->authenticate(username, password);
  }

  /**
   * Request HTTP Basic Authentication
   * 
   * AsyncWebServer doesn't have a built-in requestAuthentication() method like
   * the synchronous WebServer. We need to manually send a 401 response with
   * the WWW-Authenticate header to trigger the browser's auth dialog.
   */
  void requestAuthentication() override
  {
    AsyncWebServerResponse* response = this->_request->beginResponse(401, "text/plain", "Unauthorized");
    response->addHeader("WWW-Authenticate", "Basic realm=\"IotWebConf\"");
    this->_request->send(response);
  }

  /**
   * Check if a request argument exists
   */
  bool hasArg(const String& name) override
  {
    return this->_request->hasArg(name);
  }

  /**
   * Get a request argument value
   */
  String arg(const String name) override
  {
    return this->_request->arg(name);
  }

  /**
   * Send a response header
   * Note: In AsyncWebServer, headers must be added before sending the response.
   * We store headers and apply them when send() is called.
   */
  void sendHeader(const String& name, const String& value, bool first = false) override
  {
    // Store header for later application
    // AsyncWebServer requires headers to be set on the response object
    // We'll apply these in the send() method
    _pendingHeaders.push_back({name, value});
  }

  /**
   * Set the content length for the response
   * Note: Stored for use when building the response
   */
  void setContentLength(const size_t contentLength) override
  {
    _contentLength = contentLength;
    _contentLengthSet = true;
  }

  /**
   * Send a complete HTTP response
   * @param code - HTTP status code
   * @param content_type - MIME type (optional)
   * @param content - Response body (optional)
   */
  void send(int code, const char* content_type = nullptr, const String& content = String("")) override
  {
    AsyncWebServerResponse* response;
    
    if (content_type != nullptr)
    {
      response = this->_request->beginResponse(code, content_type, content);
    }
    else
    {
      response = this->_request->beginResponse(code);
    }

    // Apply any pending headers
    for (auto& header : _pendingHeaders)
    {
      response->addHeader(header.first, header.second);
    }
    _pendingHeaders.clear();

    // Set content length if specified
    if (_contentLengthSet)
    {
      response->setContentLength(_contentLength);
      _contentLengthSet = false;
    }

    this->_request->send(response);
  }

  /**
   * Send chunked response content
   * 
   * For chunked responses, AsyncWebServer uses a different pattern than the
   * synchronous server. We need to use beginChunkedResponse() and write chunks.
   * 
   * Note: This is a simplified implementation. For true streaming, you would
   * need to use AsyncWebServerResponse with callbacks. However, IotWebConf
   * typically sends the full response in memory, so this approach works.
   * 
   * If this is the first call to sendContent after headers have been set,
   * we'll accumulate content and send it all at once.
   */
  void sendContent(const String& content) override
  {
    // Accumulate content for chunked sending
    _chunkedContent += content;
  }

  /**
   * Stop/close the connection
   * 
   * AsyncWebServer manages connection lifetime automatically.
   * However, if we have accumulated chunked content, send it now.
   */
  void stop() override
  {
    // If we accumulated chunked content, send it now
    if (_chunkedContent.length() > 0)
    {
      AsyncWebServerResponse* response = this->_request->beginResponse(200, "text/html", _chunkedContent);
      
      // Apply any pending headers
      for (auto& header : _pendingHeaders)
      {
        response->addHeader(header.first, header.second);
      }
      _pendingHeaders.clear();

      if (_contentLengthSet)
      {
        response->setContentLength(_contentLength);
        _contentLengthSet = false;
      }

      this->_request->send(response);
      _chunkedContent = "";
    }
    
    // AsyncWebServer handles connection closure automatically
    // No explicit action needed
  }

private:
  AsyncWebServerRequest* _request;
  
  // State for managing headers and chunked responses
  std::vector<std::pair<String, String>> _pendingHeaders;
  String _chunkedContent = "";
  size_t _contentLength = 0;
  bool _contentLengthSet = false;
  
  friend IotWebConf;
};

/**
 * AsyncWebServerWrapper - Wraps AsyncWebServer for IotWebConf compatibility
 * 
 * This class adapts ESPAsyncWebServer to the WebServerWrapper interface.
 * Key differences from synchronous WebServer:
 * 
 * 1. No handleClient() needed: AsyncWebServer uses event-driven architecture
 * 2. begin() just starts the server: Route registration happens separately
 */
class AsyncWebServerWrapper : public WebServerWrapper
{
public:
  /**
   * Constructor - wraps an AsyncWebServer pointer
   * @param server - Pointer to the async server instance
   */
  AsyncWebServerWrapper(AsyncWebServer* server) { this->_server = server; }

  /**
   * Handle client requests
   * 
   * AsyncWebServer is event-driven and handles requests automatically in the
   * background. This method is called in the main loop for synchronous servers,
   * but is a no-op for async servers.
   */
  void handleClient() override
  {
    // No-op: AsyncWebServer handles requests automatically
    // The server runs in the background using async TCP handlers
  }

  /**
   * Start the web server
   * 
   * Calls the underlying AsyncWebServer's begin() method.
   * Note: All route handlers must be registered BEFORE calling begin()
   */
  void begin() override
  {
    this->_server->begin();
  }

private:
  /**
   * Private default constructor - prevents misuse
   * Only IotWebConf should be able to create instances
   */
  AsyncWebServerWrapper() { }
  
  AsyncWebServer* _server;
  
  friend IotWebConf;
};

} // end namespace

#endif // defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)

#endif // IotWebConfAsyncWebServer_h
