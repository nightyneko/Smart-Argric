#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "AZE02";
const char* password = "qqqqqqqq";
const char* mqtt_server = ""; 
const char* mqtt_user = "zero";
const char* mqtt_pass = "hello123456";

WiFiClient espClient;
PubSubClient client(espClient);

// Hardware Serial 2 pins to connect to STM32 (UART1 on STM32)
#define RXD2 25
#define TXD2 26

// MQTT Callback Function
void callback(char* topic, byte* payload, unsigned int length) {
  String commandFromNodeRed = "";
  for (int i = 0; i < length; i++) {
    commandFromNodeRed += (char)payload[i];
  }
  
  if (String(topic) == "stm32/commands") {
    Serial.print("Node-RED says: ");
    Serial.println(commandFromNodeRed);
    // Forward command to STM32
    Serial2.println(commandFromNodeRed); 
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    // --- AUTHENTICATION ADDED HERE ---
    if (client.connect("ESP32_Plant_Bridge", mqtt_user, mqtt_pass)) {
      Serial.println("connected!");
      client.subscribe("stm32/commands");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200); // USB Debugging
  Serial2.setRxBufferSize(512);
  Serial2.begin(152000, SERIAL_8N1, RXD2, TXD2); // STM32 Connection

  Serial.print("Connecting to ");  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  // Check for data from STM32 to publish to Node-RED
  if (Serial2.available()) {
    String dataFromSTM32 = Serial2.readStringUntil('\n');
    dataFromSTM32.trim(); 
    
    if (dataFromSTM32.length() > 0) {
      int startIndex = dataFromSTM32.indexOf('{');
      int endIndex = dataFromSTM32.lastIndexOf('}');

      if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
        
        String cleanJson = dataFromSTM32.substring(startIndex, endIndex + 1);
        
        Serial.print("From STM32 (Cleaned): ");
        Serial.println(cleanJson);
        
        // Publish the cleaned JSON to MQTT
        client.publish("stm32/sensors", cleanJson.c_str());
        
      } else {
        // Optional: Log the ignored garbage data for debugging
        Serial.print("Ignored non-JSON data: ");
        Serial.println(dataFromSTM32);
      }
    }
  }
}
