#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#define POT_PIN           A0    //вход потенциометра для ручной установки
#define SPINDEL_PIN       2     //включение шпинделя, логика 0/1
#define RPM_PIN           3     //прерывание датчика оборотов
#define PWM_OUT_PIN       10    //пин выхода PWM на силовую часть
#define PWM_OUT_MAX_LIMIT 215   //лимит для ограничения максимальной мощности ШИМ 0..1024
#define PWM_MIN_LIMIT     30   //вход ниже этого значения установит выход ШИМ на 0
#define DISPLAY_DIV       20    //делитель для более редкого вывода на дисплей, X * 10msec

volatile uint16_t rpm, rev, pwm;
float pwm_filtered;
boolean grbl_spindel = false;
uint32_t measureTime = 0;
uint8_t display_div = DISPLAY_DIV; //30 * delay(10) = 300msec

#define I2C_ADDRESS 0x3C
#define RST_PIN -1
SSD1306AsciiWire oled;
 
void addRevolution() {
  rev++;
}

void setup() {
  //Пины D9 и D10 - 7.8 кГц 10bit
  TCCR1A = 0b00000011;  // 10bit
  TCCR1B = 0b00000001;  // x1 phase correct
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SPINDEL_PIN, INPUT); //шпиндель с платы grbl
  pinMode(RPM_PIN, INPUT); //датчик оборотов имппульсный  
  attachInterrupt(1, addRevolution, RISING);
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(font8x8);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  //вычисление rpm
  if(display_div == 0) { //читаем пореже, для стабильности показаний
    noInterrupts();
    rpm = (rev * 60000 / (millis() - measureTime));
    rev = 0;
    measureTime = millis();
    interrupts();
  }
  //сигнал на шпиндель с grbl
  grbl_spindel = digitalRead(SPINDEL_PIN);
  
  //чтение потенциометра и приведение к диапазону
  pwm = map(analogRead(POT_PIN), 0, 1023, 0, PWM_OUT_MAX_LIMIT);
  //фильтрация для плавности
  float k;
  if (abs(pwm - pwm_filtered) > 50) k = 0.8; else k = 0.1;
  pwm_filtered += (pwm - pwm_filtered) * k;
  
  if(pwm < PWM_MIN_LIMIT ||  !grbl_spindel) { //если поценциометр выкручен ниже минимума или запрет шпинделя от grbl
    pwm_filtered = 0; //отключаем
  }
  //вывод
  analogWrite(PWM_OUT_PIN, constrain(pwm_filtered, 0, PWM_OUT_MAX_LIMIT));
  
  //вывод на дисплей
  if(display_div == 0) {
    oled.set2X();
    oled.home();
    oled.print(((uint16_t)rpm / 10) * 10);
    oled.println(F("     "));
    oled.set1X();
    oled.print("sp: ");
    oled.print(grbl_spindel ? "on" : "off");
    oled.print(" pt: ");
    oled.print((uint16_t)pwm_filtered);
    display_div = DISPLAY_DIV;
  } else {
    display_div--;
  }
}
