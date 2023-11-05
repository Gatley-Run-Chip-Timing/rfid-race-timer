#include "Unit_UHF_RFID.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"
#include "time.h"

void connectToWiFi() {
    Serial.println("(Re)connecting to WiFi...");
    WiFi.begin(SSID, PASSWORD);
}

unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return 0;
    }
    time(&now);
    return now;
}

void buzz(int ms) {
    digitalWrite(14, HIGH);
    delay(ms);
    digitalWrite(14, LOW);
}

long getTimeFromServer() {
    long epochTime = getTime();
    while (epochTime == 0) {
        for (int i = 0; i < 3; i++) {
            epochTime = getTime();

            if (epochTime != 0) {
                break;
            }

            delay(100);
        }

        connectToWiFi();
        delay(200);
    }

    return epochTime;
}

void flash() {
    for (int j = 0; j < 3; j++) {
        delay(100);
        digitalWrite(26, LOW);
        digitalWrite(27, LOW);
        delay(100);
        digitalWrite(26, HIGH);
        digitalWrite(27, HIGH);
    }
}

const char *ntpServer = "pool.ntp.org";

Unit_UHF_RFID uhf;
String info = "";
int mode = 0;
int pressed = 0;
int buttonState = 0;
int uploadMode = 0;

void setup() {
    Serial.begin(115200);

    pinMode(14, OUTPUT);
    pinMode(26, OUTPUT);
    pinMode(27, OUTPUT);
    pinMode(27, OUTPUT);
    pinMode(15, INPUT);
    pinMode(34, INPUT);

    uhf.begin(&Serial2, 115200, 16, 17, false);
    while (1) {
        info = uhf.getVersion();
        if (info != "ERROR") {
            Serial.println(info);
            break;
        }
    }

    if (digitalRead(15) == HIGH) {
        uhf.setTxPower(3000);
        Serial.println("WARNING: booting in write mode");
        uploadMode = 1;

        flash();
        flash();
        flash();
        buzz(2000);
        return;
    } else {
        uhf.setTxPower(7000);
    }

    flash();
    digitalWrite(26, LOW);
    digitalWrite(27, LOW);

    WiFi.mode(WIFI_STA);
    connectToWiFi();

    configTime(0, 0, ntpServer);
    long currentTime = getTimeFromServer();
    Serial.printf("Got time: %d\n", currentTime);
    buzz(2000);
}

String hashBuffer[300] = {};
long timeBuffer[300] = {};
String writeBuffer[300] = {};
int writePointer = 0;

uint8_t readBuffer[4] = {};

int uploadIndex = 1001;
int writtenThis = 0;

int delayCount = 0;

void loop() {
    if (uploadMode == 1) {
        buttonState = digitalRead(34);
        if (buttonState == HIGH && pressed == 0) {
            uploadIndex += 1;
            writtenThis = 0;
            Serial.printf("Now writing: %d\n", uploadIndex);

            pressed = 1;
        }

        if (buttonState == LOW) {
            pressed = 0;
        }

        uint8_t dataToWrite[4] = {uploadIndex / 256, uploadIndex & 0x000000ff};

        if (writtenThis == 0) {
            if (uhf.writeCard(dataToWrite, sizeof(dataToWrite), 0x04, 0, 0x00000000)) {
                Serial.printf("Successfully written %d!\n", uploadIndex);
                writtenThis = 1;
            } else {
                delay(100);
            }
        }
        return;
    }

    if (delayCount > 0) {
        delayCount -= 1;
    }

    if (mode != 1) {
        digitalWrite(26, HIGH);
    } else {
        digitalWrite(26, LOW);
    }

    if (mode != 0) {
        digitalWrite(27, HIGH);
    } else {
        digitalWrite(27, LOW);
    }

    buttonState = digitalRead(34);
    if (buttonState == HIGH && pressed == 0) {
        mode += 1;

        if (mode == 3) {
            mode = 0;
        }

        pressed = 1;
    }

    if (buttonState == LOW) {
        pressed = 0;
    }

    if (mode == 2) {
        flash();
        int isSuccess = 0;

        if (WiFi.status() == WL_CONNECTED) {
            WiFiClientSecure *client = new WiFiClientSecure;

            if (client) {
                client->setInsecure();

                HTTPClient http;

                for (int i = 0; i < writePointer; i++) {
                    http.begin(*client, ADD_TIME_CAPTURE_URL);
                    http.addHeader("Content-Type", "application/json");

                    int httpResponseCode = http.POST(writeBuffer[i]);

                    http.end();

                    flash();
                }

                buzz(1000);

                writePointer = 0;
                mode = 0;

                isSuccess = 1;
            }
        }

        if (delayCount == 0 && isSuccess == 0) {
            connectToWiFi();
            delayCount = 20;
        }
    } else {
        if (uhf.readCard(readBuffer, sizeof(readBuffer), 0x04, 0, 0x00000000)) {
            int numBuffer = readBuffer[0] * 256 + readBuffer[1];

            String bufferHere = String(numBuffer);
            String position = mode == 0 ? "T0" : "T1";

            String hashHere = bufferHere + position;
            long epochTime = getTimeFromServer();
            int good = 1;
            for (int j = 0; j < writePointer; j++) {
                if (hashBuffer[j] == hashHere) {
                    if (mode == 0 || epochTime - timeBuffer[j] < 60) { // 60-second cool-down
                        good = 0;
                        break;
                    }
                }
            }

            if (good == 1) {
                buzz(50);

                String json = "{\"key\": \"" + API_KEY + "\", \"position\": \"" + position + "\", \"unixEpoch\": " +
                              String(epochTime) + ", \"rfidId\": \"" + bufferHere + "\"}";

                hashBuffer[writePointer] = hashHere;
                writeBuffer[writePointer] = json;
                timeBuffer[writePointer] = epochTime;
                writePointer += 1;
            }
        }
    }

    delay(10);
}