/*
 * IntelliCart Project by Khushi Gajjar
 * Project Overview:
 * IntelliCart is a smart shopping cart system designed to transform the shopping experience by automating the checkout process and minimizing queue times.
 * Using an RFID card, customers can start a shopping session with just a tap. 
 * As they add items to the cart, IntelliCart utilizes an integrated camera to detect and recognize products visually, 
 * which are then displayed on the mobile app in real time. 
 * This app also provides a live view of the cart contents, allowing customers to keep track of selected items, 
 * view product details, and manage their shopping list seamlessly. 
 * At checkout, the cart automatically calculates the total, making the payment process smooth and efficient.
 */


#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <map>

#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define RST_PIN 33
#define SS_PIN 5

#define RED_LED 25
#define GREEN_LED 26
#define YELLOW_LED 27

const char* ssid = "Pintu_jr";
const char* password = "00000000";

WiFiServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 rfid(SS_PIN, RST_PIN);

String header;
String itemListHTML = "";
int totalAmount = 0;
const long timeoutTime = 2000; // Timeout for client connection

bool shoppingStarted = false;
int itemNumber = 1; // Start item number from 1

// Map of items and their prices
std::map<String, int> priceList = {
    {"lotion", 110}, {"necklace", 500}, {"chain", 500}, {"mouse", 300},
    {"envelope", 5}, {"lipstick", 200}, {"lip rouge", 200}, {"ballpen", 20}, {"tennis ball", 50},
    {"Band Aid", 2}, {"safety pin", 5}
};

// Track items and quantities
std::map<String, int> shoppingCart;

// Function declarations
void handleClient();
void updateQuantityAndValueInHTML(String itemName, int quantity, int price);
void blinkGreenLED();

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(YELLOW_LED, OUTPUT);
    digitalWrite(RED_LED, HIGH);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.setRotation(2); // Rotate OLED 180 degrees
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    display.println("InteliCart");
    display.setTextSize(1);
    display.setCursor(25, 40);
    display.println("Tap to Start");
    display.display();

    SPI.begin();
    rfid.PCD_Init();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
}

void loop() {
    if (!shoppingStarted && rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        shoppingStarted = true;
        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(25, 10);
        display.println("Start Shopping");
        display.setCursor(40, 30);
        display.println("Go to IP:");
        display.setCursor(20, 50);
        display.println(WiFi.localIP());
        display.display();

        rfid.PICC_HaltA();
    }

    if (Serial2.available()) {
        String itemName = Serial2.readStringUntil('\n');
        itemName.trim(); // Remove any leading/trailing whitespace
        Serial.println("Received: " + itemName);

        if (itemName.length() > 0 && priceList.find(itemName) != priceList.end()) {
            int price = priceList[itemName];
            int value = 0;

            // Check if item is already in the shopping cart
            if (shoppingCart.find(itemName) == shoppingCart.end()) {
                value = price;
                // New item
                shoppingCart[itemName] = 1; // Initialize quantity
                itemListHTML += "<tr id='" + itemName + "'><td id='iid'>" + String(itemNumber++) + "</td><td id='" + itemName + "'>" + itemName + "</td><td id='qty'>1</td><td id='prc'>" + String(price) + "</td><td id='tpr'>" + String(value) + "</td></tr>";
            } else {
                // Existing item, update quantity and value
                shoppingCart[itemName] += 1; // Increment quantity
                updateQuantityAndValueInHTML(itemName, shoppingCart[itemName], price); // Update HTML
            }

            totalAmount += price; // Update total amount
            blinkGreenLED();
        } else {
            digitalWrite(RED_LED, HIGH);
            delay(500);
            digitalWrite(RED_LED, LOW);
        }
    }
    handleClient();
}

void updateQuantityAndValueInHTML(String itemName, int quantity, int price) {
    int startPos = itemListHTML.indexOf("<td id='" + itemName + "'>");
    if (startPos != -1) {
        int quantityPos = itemListHTML.indexOf("<td id='qty'>", startPos + itemName.length() + 5);
        if (quantityPos != -1) {
            int endPos = itemListHTML.indexOf("</td>", quantityPos);
            String quantityStr = "<td id='qty'>" + String(quantity);
            itemListHTML.replace(itemListHTML.substring(quantityPos, endPos), quantityStr); // Update quantity

            int totalValue = price * quantity; // Calculate new total price for this item
            int valuePos = itemListHTML.indexOf("<td id='tpr'>", endPos);
            if (valuePos != -1) {
                int valueEndPos = itemListHTML.indexOf("</td>", valuePos);
                itemListHTML.replace(itemListHTML.substring(valuePos, valueEndPos), "<td id='tpr'>" + String(totalValue)); // Update value
            }
        }
    }
}

void blinkGreenLED() {
    for (int i = 0; i < 2; i++) {
        digitalWrite(GREEN_LED, HIGH);
        delay(200);
        digitalWrite(GREEN_LED, LOW);
        delay(200);
    }
}

void handleClient() {
    WiFiClient client = server.available();
    if (client) {
        unsigned long currentTime = millis();
        unsigned long previousTime = currentTime;
        Serial.println("New Client.");
        String currentLine = "";

        while (client.connected() && currentTime - previousTime <= timeoutTime) {
            currentTime = millis();
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                header += c;
                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<style>");
                        client.println("html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println("table { width: 80%; margin: auto; border-collapse: collapse;}");
                        client.println("table, th, td { border: 1px solid black; padding: 10px;}");
                        client.println("th { background-color: #4CAF50; color: white;}");
                        client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; font-size: 20px; cursor: pointer;}");
                        client.println(".total { font-weight: bold; }");
                        client.println("</style></head>");
                        client.println("<body><h1>Intellicart Shopping List</h1>");
                        client.println("<table><tr><th>Number</th><th>Item Name</th><th>Quantity</th><th>Price</th><th>Value</th></tr>");
                        client.println(itemListHTML);
                        client.println("</table>");
                        client.println("<p class='total'>Total Amount: Rs." + String(totalAmount) + "</p>");
                        client.println("<button class='button' onclick=\"alert('Happy Shopping!')\">Complete Shopping</button>");
                        client.println("<script>setTimeout(function(){ location.reload(); }, 5000);</script>"); // Auto-refresh every 5 seconds
                        client.println("</body></html>");

                        client.println();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c; 
                }
            }
        }
        header = "";
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
