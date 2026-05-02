/*
 * ============================================================================
 *  ESP32 MQTT Client — Датчик расстояния HC-SR04
 *  Для курсовой работы: MQTT Project (Rust Server + ESP32)
 * ============================================================================
 *  
 *  Подключения:
 *  - HC-SR04 VCC  → 5V
 *  - HC-SR04 GND  → GND
 *  - HC-SR04 Trig → GPIO 5
 *  - HC-SR04 Echo → GPIO 18 (через делитель 1кОм + 2кОм!)
 *  
 *  ВНИМАНИЕ: Echo выдаёт 5В! ESP32 работает от 3.3В!
 *  Используйте делитель напряжения, иначе сожжёте плату!
 * ============================================================================
 */

#define MQTT_MAX_PACKET_SIZE 512

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════════════════════
// НАСТРОЙКИ
// ═══════════════════════════════════════════════════════════════════════════

// Wi-Fi сеть
const char* WIFI_SSID = "iPhone (Кирилл)";
const char* WIFI_PASSWORD = "87654321";

// MQTT сервер (IP компьютера с Rust сервером)
const char* MQTT_SERVER = "172.20.10.3";
const int MQTT_PORT = 1883;

// MQTT топики
const char* MQTT_CLIENT_ID_BASE = "esp32_sensor_01";
const char* MQTT_TOPIC = "esp32/distance";
const char* MQTT_STATUS_TOPIC = "status/esp32";

// Пины датчика HC-SR04
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// Интервал отправки данных (мс)
const unsigned long SEND_INTERVAL = 1000;

// Таймауты и задержки
const unsigned long WIFI_TIMEOUT = 10000;
const unsigned long MQTT_RETRY_DELAY = 3000;

// ═══════════════════════════════════════════════════════════════════════════

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastSendTime = 0;
unsigned long lastReconnectAttempt = 0;
bool wifiConnected = false;

// ───────────────────────────────────────────────────────────────────────────
// Функция измерения расстояния (HC-SR04)
// ───────────────────────────────────────────────────────────────────────────
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1.0;
  
  // Скорость звука: 0.034 см/мкс
  return (duration * 0.034) / 2.0;
}

// ───────────────────────────────────────────────────────────────────────────
// Подключение к Wi-Fi
// ───────────────────────────────────────────────────────────────────────────
bool connectWiFi() {
  Serial.print("Подключение к Wi-Fi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi подключен!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nОшибка Wi-Fi");
    WiFi.disconnect();
    return false;
  }
}

// ───────────────────────────────────────────────────────────────────────────
// Описание ошибки MQTT
// ───────────────────────────────────────────────────────────────────────────
String getMqttErrorString(int state) {
  switch(state) {
    case MQTT_CONNECTION_TIMEOUT:     return "Timeout";
    case MQTT_CONNECTION_LOST:        return "Lost";
    case MQTT_CONNECT_FAILED:         return "Connect failed";
    case MQTT_DISCONNECTED:           return "Disconnected";
    case MQTT_CONNECT_BAD_PROTOCOL:   return "Bad protocol";
    case MQTT_CONNECT_BAD_CLIENT_ID:  return "Bad ID";
    case MQTT_CONNECT_UNAVAILABLE:    return "Unavailable";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "Bad credentials";
    case MQTT_CONNECT_UNAUTHORIZED:   return "Unauthorized";
    default:                          return "Unknown #" + String(state);
  }
}

// ───────────────────────────────────────────────────────────────────────────
// Подключение к MQTT брокеру (с фиксом синхронизации состояния)
// ───────────────────────────────────────────────────────────────────────────
bool connectMQTT() {
  if (mqttClient.connected()) return true;
  if (millis() - lastReconnectAttempt < MQTT_RETRY_DELAY) return false;
  lastReconnectAttempt = millis();

  // Уникальный ID при каждом запуске (предотвращает конфликты сессий)
  String clientId = String(MQTT_CLIENT_ID_BASE) + "_" + String(random(0x1000, 0xFFFF), HEX);
  Serial.print("MQTT connect ("); Serial.print(clientId); Serial.print(")... ");
  
  // Попытка подключения с Last Will и cleanSession=true
  bool tcpOk = mqttClient.connect(
    clientId.c_str(),           // client_id
    nullptr, nullptr,           // user, pass
    MQTT_STATUS_TOPIC,          // will_topic
    0, true, "offline",         // will_qos, will_retain, will_msg
    true                        // ← clean_session = true
  );
  
  if (tcpOk) {
    // Вызываем loop() несколько раз, чтобы состояние стало connected() == true
    for(int i = 0; i < 12; i++) {
      mqttClient.loop();
      delay(40);
    }
    
    if(mqttClient.connected()) {
      Serial.println("OK");
      mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
      return true;
    } else {
      Serial.println("Сброшено после рукопожатия");
      return false;
    }
  } else {
    int state = mqttClient.state();
    Serial.print("FAIL rc=");
    Serial.println(getMqttErrorString(state));
    return false;
  }
}

// ───────────────────────────────────────────────────────────────────────────
// Отправка данных в MQTT
// ───────────────────────────────────────────────────────────────────────────
void sendDistance(float distance) {
  StaticJsonDocument<128> doc;
  doc["distance"] = distance;
  doc["timestamp"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  Serial.print("Отправка: "); Serial.println(payload);
  
  if (mqttClient.publish(MQTT_TOPIC, payload.c_str())) {
    Serial.println("Успешно отправлено");
  } else {
    Serial.println("Ошибка отправки");
  }
}

// ───────────────────────────────────────────────────────────────────────────
// Обработка входящих MQTT сообщений
// ───────────────────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Получено ["); Serial.print(topic); Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

// ───────────────────────────────────────────────────────────────────────────
// Проверка и восстановление соединений
// ───────────────────────────────────────────────────────────────────────────
void checkConnections() {
  // Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi потеряно, переподключение...");
    delay(1000);
    wifiConnected = connectWiFi();
    if (!wifiConnected) return;
  }
  
  // MQTT
  if (!mqttClient.connected()) {
    Serial.println("MQTT потеряно, переподключение...");
    connectMQTT();
  }
  
  mqttClient.loop();
}

// ═══════════════════════════════════════════════════════════════════════════
// SETUP — Инициализация
// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000); // Ждём инициализацию USB

  
  // Инициализация генератора случайных чисел
  randomSeed(millis() + analogRead(0));
  
  // Пины датчика
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  
  // 1. Подключаемся к Wi-Fi
  wifiConnected = connectWiFi();
  if (!wifiConnected) {
    Serial.println("Нет сети. Перезагрузите плату.");
    return;
  }
  
  // 2. Настраиваем MQTT клиент
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  // 3. Умное подключение к брокеру (до 5 секунд попыток)
  //    Это решает проблему "сервер запущен раньше -> не подключается"
  Serial.println("Подключение к брокеру (ожидание до 5 сек)...");
  unsigned long connStart = millis();
  bool mqttReady = false;
  while(millis() - connStart < 5000) {
    if(connectMQTT()) {
      mqttReady = true;
      break;
    }
    delay(500);
    yield(); // Разрешаем фоновым задачам Wi-Fi работать
  }
  
  if(mqttReady) Serial.println("MQTT готов к работе!");
  else          Serial.println("Брокер недоступен. Попытки продолжатся в цикле.");
  
  Serial.println("Инициализация завершена!\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// LOOP — Главный цикл
// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  checkConnections();
  
  // Отправка данных только при активном MQTT
  if (wifiConnected && mqttClient.connected()) {
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= SEND_INTERVAL) {
      lastSendTime = currentTime;
      
      float distance = measureDistance();
      if (distance >= 0) {
        Serial.print("Расстояние: "); Serial.print(distance); Serial.println(" см");
        sendDistance(distance);
      } else {
        Serial.println("Объект вне диапазона");
      }
      Serial.println();
    }
  }
  
  delay(10);
}