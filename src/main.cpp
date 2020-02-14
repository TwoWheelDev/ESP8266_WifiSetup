#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#ifndef APSSID
#define APSSID "SmartHome WiFi Sensor"
#define APPSK "Passw0rd"
#endif

// WiFi Credentials
const char *ssid = APSSID;
const char *password = APPSK;

ESP8266WebServer server(80);

// Root Page when device is connected to WiFi as a station
void handleRoot() {
    String htmlPage = String("<!DOCTYPE HTML><html><body><h1>SmartHome WiFi Sensor</h1><p>Reset WiFi Configuration:</p>");
    htmlPage += String("<a href='/resetWiFi'><button>Reset WiFi</button></a>");
    htmlPage += String("</body></html>");
    server.send(200, "text/html", htmlPage);
}
// Root Page when device is NOT connected to WiFi as a station e.g Setup mode
void handleRootFirstRun() {
    Serial.println("Scanning for networks");
    int numSsid = WiFi.scanNetworks();
    Serial.print(numSsid);
    Serial.println(" networks found");
    String htmlPage = String("<!DOCTYPE HTML><html><body><h1>WiFi Networks</h1><p>WiFi Network:</p>");
    if (numSsid == 0) {
        htmlPage += String("<h2>No Networks found</h2>");
    } else {
        /* -----------------------------------------------------------------------------
         * Sort networks by their RSSI strength
         * Source: https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp#L489
         * ----------------------------------------------------------------------------- */
        //sort networks
        int indices[numSsid];
        for (int i = 0; i < numSsid; i++) {
            indices[i] = i;
        }

        // RSSI SORT
        for (int i = 0; i < numSsid; i++) {
            for (int j = i + 1; j < numSsid; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    std::swap(indices[i], indices[j]);
                }
            }
        }

        htmlPage += String("<form action='/connectNetwork' method='post'>");
        for (int i = 0; i < numSsid; i++) {
            String currentSsid = WiFi.SSID(indices[i]);
            int currentRSSI = WiFi.RSSI(indices[i]);
            String currentEnc = "";
            if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) { currentEnc = "&#128274; "; }

            htmlPage += String("<input type='radio' id='" + currentSsid + "' name='ssid' value='" + currentSsid + "'>");
            htmlPage += String("<label for='" + currentSsid + "'>" + currentEnc +
                    currentSsid + " (" + currentRSSI + "dBm)</label><br>");
        }
        htmlPage += String("</select><br>");
        htmlPage += String(
                "<label for='password'>Password: </label><input id='password' name='password' type='password'>");
        htmlPage += String("<input type='submit' value='Connect to Network'></form>");
    }
    htmlPage += String("</body></html>");
    server.send(200, "text/html", htmlPage);
}

int connectNetwork(const String& netSSID, const String& netPassword) {
    // Connect to the specified network
    WiFi.begin(netSSID, netPassword);
    int connStatus = WiFi.waitForConnectResult();

    return connStatus;
}

void handleConnectNetwork() {
    // Handle the form submission to connect to a network
    // The htmlPages in this function never display... Probably due to changing networks, can be tidied up
    if(server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method not allowed");
    } else {
        Serial.println("Received valid POST request, attempting network connection");
        int connResult = connectNetwork(server.arg("ssid"), server.arg("password"));
        Serial.print("Connection Result: ");
        Serial.println(connResult);
        switch(connResult) {
            case WL_CONNECTED:
                Serial.println("Connection successful");
                Serial.print("Local IP: ");
                Serial.println(WiFi.localIP());
                break;
            case WL_CONNECT_FAILED:
                WiFi.disconnect();
                Serial.println("Connection unsuccessful. Check the password");
                break;
            default:
                Serial.println("Connection unsuccessful. Unknown Error");
        }
        Serial.println("Send the response to the browser");
        ESP.restart(); // Restart the ESP to ensure we are connected to the Wifi and the SoftAP has been disabled
    }
}

void handleReset() {
    /* Disconnect from the current access point and restart the ESP to rerun setup */
    WiFi.disconnect();
    ESP.restart();
}

void setup() {
    delay(100);
    Serial.begin(115200);
    Serial.println();

    if (!WiFi.isConnected()) {
        WiFi.mode(WIFI_AP_STA);
        Serial.println("Configuring access point");
        WiFi.softAP(ssid, password);

        IPAddress myIP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(myIP);

        server.on("/", handleRootFirstRun);
        server.on("/connectNetwork", handleConnectNetwork);
    } else {
        WiFi.mode(WIFI_STA);
        Serial.println(WiFi.localIP());
        server.on("/", handleRoot);
        server.on("/resetWiFi", handleReset);
    }

    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // Based on: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266mDNS/examples/mDNS_Web_Server/mDNS_Web_Server.ino#L53
    if (!MDNS.begin("esp8266")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        Serial.println("mDNS responder started");
    }

    server.begin();
    Serial.println("HTTP Server Started");

    MDNS.addService("http", "tcp", 80);
}

void loop() {
    MDNS.update();
    server.handleClient();
}