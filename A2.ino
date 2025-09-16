#include "esp_camera.h"
#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

// Replace with your bot token and chat ID
#define BOT_TOKEN ""
#define CHAT_ID ""

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#define FLASH_GPIO 4
#include "camera_pins.h"


// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "";
const char *password = "";

const int debounceDelay = 52; // Delay in milliseconds
const int motionDetectionThreshold = 30; // Number of consistent HIGH readings

// Create a secure client instance
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Motion sensor pin
const int motionSensorPin = 12; 

void startCameraServer();

// Variable to track the system's active state
bool light = false;
bool systemActive = false;
bool takephoto = false;
bool flashOnForPicture = false;

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(motionSensorPin, INPUT); // Set motion sensor pin as input
  // Configure flash pin
  pinMode(FLASH_GPIO, OUTPUT);
  digitalWrite(FLASH_GPIO, LOW); // Ensure the flash is off initially

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; //hz
  config.frame_size = FRAMESIZE_HVGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 6;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s,2);
    s->set_awb_gain(s, 0);
    s->set_framesize(s, FRAMESIZE_HVGA);
    s->set_saturation(s, -2);

  }


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  client.setInsecure();

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    Serial.println("Received message: " + text);

    if (text == "/stop" && chat_id == CHAT_ID) {
      systemActive = false;
      bot.sendMessage(CHAT_ID, "System deactivated. Camera and motion detection are now off.", "");
      Serial.println("System deactivated.");
    } else if (text == "/start" && chat_id == CHAT_ID) {
      systemActive = true;
      bot.sendMessage(CHAT_ID, "System reactivated. Camera and motion detection are now on.", "");
      Serial.println("System reactivated.");
    } else if (text == "/flashon" && chat_id == CHAT_ID) {
      flashOnForPicture = true;
      bot.sendMessage(CHAT_ID, "Flash is now ON.", "");
      Serial.println("Flash turned on.");
    } else if (text == "/flashoff" && chat_id == CHAT_ID) {
      flashOnForPicture = false;
      bot.sendMessage(CHAT_ID, "Flash is now OFF.", "");
      Serial.println("Flash turned off.");
    } else if (text == "/takephoto" && chat_id == CHAT_ID) {
      takephoto = true;
    } else if (text == "/light" && chat_id == CHAT_ID) {
      if (!light) {
        light = true;
        digitalWrite(FLASH_GPIO, HIGH);  // Turn on light
        bot.sendMessage(CHAT_ID, "Light is now On.", "");
      } else {
        light = false;
        digitalWrite(FLASH_GPIO, LOW);  // Turn off light
        bot.sendMessage(CHAT_ID, "Light is now OFF.", "");
      }
    } else if (text == "/help" && chat_id == CHAT_ID) {
      String helpMessage = "Available commands:\n"
                           "/start - Reactivate the system.\n"
                           "/stop - Deactivate the system.\n"
                           "/flashon - Turn the flash ON.\n"
                           "/flashoff - Turn the flash OFF.\n"
                           "/takephoto - Takes photo for user.\n"
                           "/light - Turns light on.\n"
                           "/help - Display this help message.";
      bot.sendMessage(CHAT_ID, helpMessage, "");
      Serial.println("Help command executed.");
    } else {
      bot.sendMessage(CHAT_ID, "Not a command try /help", "");
      return;
    }
  }
}

void takePhoto() {
  // Capture and send a photo
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  if (numNewMessages > 0) {
      handleNewMessages(numNewMessages); // Process /takephoto or other commands
  }

  if (flashOnForPicture) {
    digitalWrite(FLASH_GPIO, HIGH);  // Turn on the flash
    delay(100);                      // Brief delay for the flash
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    bot.sendMessage(CHAT_ID, "Failed to capture photo.", "");
    return;
  }

  Serial.printf("Photo captured! Size: %zu bytes\n", fb->len);

  if (client.connect("api.telegram.org", 443)) {
    String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n--boundary\r\n";
    head += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--boundary--\r\n";

    client.print("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\n");
    client.print("Host: api.telegram.org\r\n");
    client.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
    client.print("Content-Length: " + String(head.length() + fb->len + tail.length()) + "\r\n");
    client.print("\r\n");

    client.print(head);
    client.write(fb->buf, fb->len);
    client.print(tail);

    Serial.println("Photo sent to Telegram.");
    
  } else {
    Serial.println("Failed to connect to Telegram server.");
  }
  client.stop();

  // Release the frame buffer
  esp_camera_fb_return(fb);

  if (flashOnForPicture) {
    digitalWrite(FLASH_GPIO, LOW);  // Turn off the flash
  }
  if (light) {
    digitalWrite(FLASH_GPIO, HIGH);
  }
  takephoto = false;
}

void loop() {
  // Check for new Telegram messages to stop the system
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  if (numNewMessages > 0) {
    handleNewMessages(numNewMessages);
  }

  if (takephoto) {
    takePhoto();
  }
  // If the system is inactive, skip motion detection and image capture
  if (!systemActive) {
    delay(1000);
    return;
  }

  // Noise-filtered motion detection
  int motionCount = 0;
  for (int i = 0; i < motionDetectionThreshold; i++) {
    if (digitalRead(motionSensorPin) == HIGH) {
      motionCount++;
    }
    delay(debounceDelay); // Short delay between readings
  }

  if (motionCount >= motionDetectionThreshold) {
    Serial.println("Motion Detected!");

    // Notify via Telegram
    if (bot.sendMessage(CHAT_ID, "Alert! Motion detected by the ESP32-CAM.", "")) {
      Serial.println("Message sent successfully");
    } else {
      Serial.println("Failed to send message");
    }

    if (flashOnForPicture) {
    digitalWrite(FLASH_GPIO, HIGH);  // Turn on the flash
    delay(100);                      
    }
    // Capture an image when motion is detected
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    Serial.printf("Picture taken! Size: %zu bytes\n", fb->len);

    // Send the captured image to Telegram
    if (client.connect("api.telegram.org", 443)) {
      String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n--boundary\r\n";
      head += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
      head += "Content-Type: image/jpeg\r\n\r\n";
      String tail = "\r\n--boundary--\r\n";

      client.print("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\n");
      client.print("Host: api.telegram.org\r\n");
      client.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
      client.print("Content-Length: " + String(head.length() + fb->len + tail.length()) + "\r\n");
      client.print("\r\n");

      client.print(head);
      client.write(fb->buf, fb->len);
      client.print(tail);

      Serial.println("Image sent to Telegram.");
    } else {
      Serial.println("Failed to connect to Telegram server.");
    }

    // Close the connection and release the frame buffer
    client.stop();
    esp_camera_fb_return(fb);

    if (flashOnForPicture) {
      digitalWrite(FLASH_GPIO, LOW);  // Turn off the flash
    }
    if (light) {
      digitalWrite(FLASH_GPIO, HIGH);
    }
    
    delay(14000);
  }

  // Add a short delay to maintain stability without missing motion events
  delay(200);
}
