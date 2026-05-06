#include <Arduino.h>  

// Captive Portal
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround


const char *ssid = "Connect-To-Portal";  // The SSID can't have a space in it.
 const char * password = "12345678"; //Atleast 8 chars
//const char *password = NULL;  // no password

#define MAX_CLIENTS 4	// ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6	// 2.4ghz channel 6 

const IPAddress localIP(4, 3, 2, 1);		   // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);		   // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage

const char index_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>ESP32 Captive Portal</title>
      <style>
        body {background-color:#06cc13;}
        h1 {color: white;}
        h2 {color: white;}
      </style>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>PORTAL!</h1>
      <h2>This is a captive portal example. All requests will be redirected here </h2>
    </body>
  </html>
)=====";

DNSServer dnsServer;
AsyncWebServer server(80);

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

	// Set the TTL for DNS response - LOW TTL for faster updates on iOS
	// iOS caches DNS, so lower TTL forces re-checks faster
	dnsServer.setTTL(60);  // Reduced from 3600 to 60 seconds for faster detection
	// Use "*" to respond to ALL domain queries and redirect them to localIP
	dnsServer.start(53, "*", localIP);
	Serial.println("DNS Server started - redirecting all domains to " + localIP.toString());
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

	// Set the WiFi mode to access point and station
	WiFi.mode(WIFI_MODE_AP);

	// Define the subnet mask for the WiFi network
	const IPAddress subnetMask(255, 255, 255, 0);

	// Configure the soft access point with a specific IP and subnet mask
	WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

	// Start the soft access point with the given ssid, password, channel, max number of clients
	WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

	// Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
	esp_wifi_stop();
	esp_wifi_deinit();
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
	my_config.ampdu_rx_enable = false;
	esp_wifi_init(&my_config);
	esp_wifi_start();
	vTaskDelay(200 / portTICK_PERIOD_MS);  // Increased delay for better stability
	
	// Print AP info to serial
	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());
	Serial.print("AP SSID: ");
	Serial.println(ssid);
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
    
    // --- 1. Notification Trigger Handlers ---
    // We serve the HTML directly here instead of redirecting to force the "Sign-in" popup
    
    // Apple Probes - iOS/macOS - CRITICAL FOR FAST DETECTION
    // iOS sends these probes frequently - must respond INSTANTLY with minimal data
    auto appleHandler = [](AsyncWebServerRequest *request) {
        // Lightweight response for faster iOS detection
        static const char minimal_html[] = "<html><body>Success</body></html>";
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", minimal_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "-1");
        response->addHeader("Content-Length", String(strlen(minimal_html)));
        request->send(response);
        Serial.println("iOS hotspot-detect.html");
    };
    
    // Multiple iOS endpoints for redundancy
    server.on("/hotspot-detect.html", HTTP_GET, appleHandler);
    server.on("/hotspot-detect.html", HTTP_POST, appleHandler);
    server.on("/hotspot-detect.html", HTTP_HEAD, appleHandler);
    server.on("/library/test/success.html", HTTP_GET, appleHandler);
    server.on("/library/test/success.html", HTTP_POST, appleHandler);

    // Android Probes
    server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) {
        // For Android: return 204 No Content to trigger portal, some devices expect redirect
        AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "");
        response->addHeader("Location", localIPURL);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
        Serial.println("Android generate_204 Triggered");
    });

    // Windows/Other Probes
    server.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { 
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Microsoft NCSI");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
        Serial.println("Windows connecttest.txt");
    });
    
    server.on("/ncsi.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { 
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "NCSI");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });
    
    server.on("/wpad.dat", HTTP_ANY, [](AsyncWebServerRequest *request) { 
        request->send(404); 
    });

    // Additional probe endpoints for better compatibility
    server.on("/ca.html", HTTP_ANY, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "");
        response->addHeader("Location", localIPURL);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    server.on("/success.txt", HTTP_ANY, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "success");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // --- 2. Main Portal Page ---
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
        Serial.println("Served Portal Home");
    });

    // --- 3. The "Every Time" Redirect (Catch-all) ---
    server.onNotFound([](AsyncWebServerRequest *request) {
        // Don't redirect for static/known content types, just serve portal
        AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "");
        response->addHeader("Location", localIPURL);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
        Serial.printf("Redirecting: %s%s\n", request->host().c_str(), request->url().c_str());
    });
}

void setup() {
	// Set the transmit buffer size for the Serial object and start it with a baud rate of 115200.
	Serial.setTxBufferSize(1024);
	Serial.begin(115200);

	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// Print a welcome message to the Serial port.
	Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");  //__DATE__ is provided by the platformio ide
	Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

	startSoftAccessPoint(ssid, password, localIP, gatewayIP);

	setUpDNSServer(dnsServer, localIP);

	setUpWebserver(server, localIP);
	server.begin();

	Serial.print("\n");
	Serial.print("Startup Time:");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
	Serial.println(millis());
	Serial.print("\n");
}

void loop() {
	dnsServer.processNextRequest();	 // Process DNS requests immediately for fast response
	delay(10);			 // Reduced from DNS_INTERVAL for faster detection (10ms instead of 30ms)
}