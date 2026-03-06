#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "192.168.1.xxx"; // Your Mosquitto IP

WiFiClient espClient;
PubSubClient client(espClient);

// Define Hardware Serial 2 pins to connect to STM32
#define RXD2 25
#define TXD2 26

// ---------------------------------------------------------
// THIS IS THE CRITICAL PART: The MQTT Callback Function
// ---------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  String commandFromNodeRed = "";
  
  // 1. The payload comes in as raw bytes. We loop through them to build a String.
  for (int i = 0; i < length; i++) {
    commandFromNodeRed += (char)payload[i];
  }
  
  // 2. Verify we are receiving on the correct topic
  if (String(topic) == "stm32/commands") {
    
    // Print to standard Serial so you can debug on your computer screen
    Serial.print("Node-RED says: ");
    Serial.println(commandFromNodeRed);

    // 3. Forward the exact string to the STM32 via Hardware Serial 2
    // Using println() automatically adds a newline character ('\n') at the end.
    // This acts as an "end of message" marker so the STM32 knows when to stop reading.
    Serial2.println(commandFromNodeRed); 
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32-Receiver")) {
      Serial.println("connected!");
      
      // ---------------------------------------------------------
      // YOU MUST SUBSCRIBE to the topic, otherwise the callback never fires
      // ---------------------------------------------------------
      client.subscribe("stm32/commands");
      
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200); // For USB debugging

  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); // Tell MQTT to use our function when data arrives
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  // client.loop() is required. It checks for incoming Wi-Fi packets 
  // and fires the callback() function if it finds one.
  client.loop(); 
  if (Serial2.available()) {
    // Read the incoming text from STM32 until the newline character
    String dataFromSTM32 = Serial2.readStringUntil('\n');
    
    // Clean up any stray carriage returns (\r)
    dataFromSTM32.trim(); 
    
    if (dataFromSTM32.length() > 0) {
      // Print to USB serial for debugging
      Serial.print("Forwarding to Node-RED: ");
      Serial.println(dataFromSTM32);
      
      // Send the data to the broker on the "stm32/sensors" topic
      // We use .c_str() because publish() requires a standard char array, not an Arduino String
      client.publish("stm32/sensors", dataFromSTM32.c_str());
    }
  }
} 