#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>

const char* ssid = "TECNO SPARK 30 Pro";
const char* password = "11234567899";
const char* server_url = "http://147.93.41.74:2121/ocr-to-speech/";

const int buttonPin = 0;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
    return;
  }

  Serial.println("🚀 Web server started!");
}

void loop() {
  if (digitalRead(buttonPin) == LOW) {
    delay(100); // debounce
    if (digitalRead(buttonPin) == LOW) {
      Serial.println("🔘 Button pressed! Capturing image...");
      captureImageFromIPWebcam();
      sendImageToServer();
    }
    while (digitalRead(buttonPin) == LOW); // wait for release
  }
}

void captureImageFromIPWebcam() {
  HTTPClient http;
  http.begin(ip_webcam_url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    File imageFile = SPIFFS.open("/snapshot.jpg", FILE_WRITE);
    if (!imageFile) {
      Serial.println("❌ Failed to open file for writing");
      http.end();
      return;
    }

    Serial.println("📥 Downloading image...");
    uint8_t buff[128] = { 0 };
    int total = 0;
    while (http.connected() && stream->available()) {
      size_t len = stream->readBytes(buff, sizeof(buff));
      imageFile.write(buff, len);
      total += len;
    }
    imageFile.close();
    Serial.printf("✅ Image Captured! Size: %d bytes\n", total);
  } else {
    Serial.printf("❌ Failed to download image. Code: %d\n", httpCode);
  }
  http.end();
}

void sendImageToServer() {
  File imageFile = SPIFFS.open("/snapshot.jpg", FILE_READ);
  if (!imageFile) {
    Serial.println("❌ Failed to open image file");
    return;
  }

  String boundary = "----ESP32Boundary";
  String contentType = "multipart/form-data; boundary=" + boundary;

  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"snapshot.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int totalSize = bodyStart.length() + imageFile.size() + bodyEnd.length();
  uint8_t* body = (uint8_t*)malloc(totalSize);
  if (!body) {
    Serial.println("❌ Not enough memory for body");
    imageFile.close();
    return;
  }

  int idx = 0;
  memcpy(body + idx, bodyStart.c_str(), bodyStart.length());
  idx += bodyStart.length();

  while (imageFile.available()) {
    body[idx++] = imageFile.read();
  }
  imageFile.close();

  memcpy(body + idx, bodyEnd.c_str(), bodyEnd.length());
  idx += bodyEnd.length();

  WiFiClient client;
  HTTPClient http;
  http.begin(client, server_url);
  http.setTimeout(5000);  // increase timeout
  http.addHeader("Content-Type", contentType);

  Serial.println("📤 Uploading image...");
  int httpCode = http.POST(body, idx);
  free(body);

  if (httpCode == 200) {
    Serial.println("✅ Upload success! Receiving MP3...");
    File mp3File = SPIFFS.open("/speech.mp3", FILE_WRITE);
    if (!mp3File) {
      Serial.println("❌ Failed to open MP3 file");
      http.end();
      return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[128];
    while (stream->available()) {
      size_t len = stream->readBytes(buf, sizeof(buf));
      mp3File.write(buf, len);
    }
    mp3File.close();
    Serial.println("✅ MP3 saved!");
  } else {
    Serial.printf("❌ Upload failed. HTTP code: %d\n", httpCode);
  }

  http.end();
}
