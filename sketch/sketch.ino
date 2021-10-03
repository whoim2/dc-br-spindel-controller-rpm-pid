#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "PID_v1.h"

#define POT_PIN           A0    //вход потенциометра для ручной установки
#define PWM_OUT_PIN       10    //пин выхода PWM на силовую часть
#define PWM_OUT_MAX_LIMIT 215   //лимит для ограничения максимальной мощности 0..1024
#define RPM_MIN_LIMIT     3000  //вход ниже этого значение установит выход ШИМ на 0, в об/мин
#define RPM_MAX           18000 //максимальное достижимое ограничение оборотов для мотора
#define DISPLAY_DIV       20     //делитель для более редкого вывода на дисплей, X * 10msec

volatile double rpm, need_rpm, rpm_out, rev;
uint32_t measureTime = 0;
uint8_t display_div = DISPLAY_DIV; //30 * delay(10) = 300msec
String mode;

#define I2C_ADDRESS 0x3C
#define RST_PIN -1
SSD1306AsciiWire oled;
PID rpmPID(&rpm, &rpm_out, &need_rpm, 0.15, 0.5, 0, DIRECT);
 
void addRevolution() {
  rev++;
}

void setup() {
  //Пины D9 и D10 - 7.8 кГц 10bit
  TCCR1A = 0b00000011;  // 10bit
  TCCR1B = 0b00000001;  // x1 phase correct
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(3, INPUT);
  
  attachInterrupt(1, addRevolution, RISING);
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(font8x8);
  rpmPID.SetMode(AUTOMATIC);
  rpmPID.SetOutputLimits(0, RPM_MAX);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  //delay(10); //задержка
  //вычисление rpm
  if(display_div == 0) { //читаем пореже, для стабильности показаний
    //digitalWrite(LED_BUILTIN, HIGH);
    noInterrupts();
    rpm = (rev * 60000 / (millis() - measureTime));
    rev = 0;
    measureTime = millis();
    interrupts();
  }
  //чтение потенциометра и приведение к диапазону об/мин
  need_rpm = map(analogRead(POT_PIN), 0, 1023, 0, RPM_MAX);
  if(need_rpm < RPM_MIN_LIMIT) {
      if(need_rpm < 50) { //deadband
        need_rpm = 0; //если меньше порога, отключаем совсем
        mode = F("OFF");
      }
      else {
        need_rpm = 500; //если от deadband до RPM_MIN_LIMIT, считываем ШИМ с grbl
        mode = F("PWM");
      }
      
  } else {
    mode = F("MANU");
  }
  rpmPID.Compute(); //вычисляем pid
  //расчет
  uint16_t pwm_out = 0;
  if(need_rpm > 50) pwm_out = map((uint16_t)rpm_out, 0, RPM_MAX, 0, PWM_OUT_MAX_LIMIT);
  //вывод на дисплей
  if(display_div == 0) {
    oled.set2X();
    //oled.clear();
    oled.home();
    oled.print(((uint16_t)rpm / 10) * 10);
    oled.println(F("     "));
    oled.set1X();
    oled.print("S:");
    oled.print((uint16_t)need_rpm);
    oled.print(" O:");
    oled.print(pwm_out);
    oled.println(F("     "));
    oled.print(mode);
    oled.println(F("   "));
    display_div = DISPLAY_DIV;
    //digitalWrite(LED_BUILTIN, LOW);
  } else {
    display_div--;
  }
    //write out
    analogWrite(PWM_OUT_PIN, pwm_out);
  
}
