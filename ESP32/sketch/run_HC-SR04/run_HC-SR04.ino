// Простой код для HC-SR04 на ESP32
#define TRIG_PIN  5    // GPIO5  - Trig
#define ECHO_PIN  18   // GPIO18 - Echo

void setup() {
  Serial.begin(115200);        // Для ESP32 лучше использовать 115200
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("=== HC-SR04 Ultrasonic Sensor на ESP32 ===");
  Serial.println("Запуск...");
  delay(1000);
}

void loop() {
  // Отправляем импульс
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Измеряем время отклика
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Расчёт расстояния в сантиметрах
  float distance_cm = duration * 0.0343 / 2;

  // Расчёт в миллиметрах и дюймах (для удобства)
  float distance_mm = distance_cm * 10;
  float distance_inch = distance_cm / 2.54;

  // Вывод в Serial Monitor
  Serial.print("Расстояние: ");
  Serial.print(distance_cm, 1);      // 1 знак после запятой
  Serial.print(" см  |  ");
  Serial.print(distance_mm, 0);
  Serial.print(" мм  |  ");
  Serial.print(distance_inch, 1);
  Serial.println(" дюймов");

  // Дополнительная информация о длительности импульса
  // Serial.print("Время импульса: ");
  // Serial.print(duration);
  // Serial.println(" мкс");

  delay(500);  // Измеряем 2 раза в секунду
}