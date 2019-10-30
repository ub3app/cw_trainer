/////////////////////////////////////////////////////////////////////////////////////////
//
//  UB3APP CW Trainer
//  GNU GPLv3 license
//  Contact: ub3app@gmail.com
//
//  Forked from:  
//
//  ARRL's Arduino for Ham Radio by Glen Popiel, KW5GP
//  Chapter 17 - Iambic Keyer
//
//  Iambic Morse Code Keyer Sketch
//  Copyright (c) 2009 Steven T. Elliott 
//
//  Modified by Glen Popiel, KW5GP
//  Based on OpenQRP Blog Iambic Morse Code Keyer Sketch by Steven T. Elliott, 
//  K1EL - Used with Permission
//
/////////////////////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ST_Pin 9                    // Пин для подключения наушников
#define LP_in 7                     // Пин правого контакт ключа
#define RP_in 5                     // Пин левого контакт ключа
#define led_Pin 13                  // Пин светодиода
#define key_Pin 6                   // Пин реле для управления трансивером

#define enc_btn_Pin 2               // Пин кнопки SW энкодера
#define enc_a_Pin 3                 // Пин контакта DT энкодера
#define enc_b_Pin 4                 // Пин контакта CLK энкодера

#define SCREEN_WIDTH 128            // Размер экрана ширина
#define SCREEN_HEIGHT 32            // Размер экрана высота
#define OLED_RESET     -1           // Если есть Rset pin на экране

int longPressTime = 500;            // Переменная длительность длительного нажатия на кнопку энкодера
int blink_time = 500;               // Переменная скорости моргания элементов меню

int ST_Freq;                        // Переменная частоты тона
int key_speed;                      // Переменная скорости телеграфа WPM
int key_mode = 0;                   // Переменная режима работы ключа - 0 = Iambic Mode A, 1 = Iambic Mode B

struct SettingsObj {                // Структура для сохранения настроек в EEPROM
  int ifreq;
  int ispeed; 
};
SettingsObj so;                     // Объект структуры

unsigned long ditTime;              // Количество миллисекунд длительности точки

char keyerState;                    // Текущее состояние работы тренажера
enum KSTYPE {IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT};

char keyerControl;                  // Состояние ключа
#define     DIT_L      0x01         // Точка нажата
#define     DAH_L      0x02         // Тире нажато
#define     DIT_PROC   0x04         // Точка все еще нажата

static long ktimer;                 // Таймер удержания контактов ключа

static long ktimer_idle_dit;        // Таймер интервала между символами
static long ktimer_idle_dah;        // Таймер интервала между словами
int idle_first_flag = 4;            // Текущие состояние бездействия

String ch = "";                     // Текущий знак в формате кода Морзе

String text1 = "";                  // Весь текст в формате Морзе
String text2 = "";                  // Весь текст в формате обычных букв

String text1_prev;                  // Переменные для определения изменения текста
String text2_prev;

const int NUMBER_OF_ELEMENTS = 46;  // Длина массива символов и кодов
const int MAX_SIZE = 7;             // Максимальная длина строки в формате кода Морзе
char codecw [NUMBER_OF_ELEMENTS] [MAX_SIZE] = {
    {".-"}, {"-..."}, {"-.-."}, {"-.."}, {"."}, {"..-."}, {"--."}, {"...."}, 
    {".."}, {".---"}, {"-.-"}, {".-.."}, {"--"}, {"-."}, {"---"}, {".--."}, 
    {"--.-"}, {".-."}, {"..."}, {"-"}, {"..-"}, {"...-"}, {".--"}, {"-..-"}, 
    {"-.--"}, {"--.."}, {".----"}, {"..---"}, {"...--"}, {"....-"}, {"....."}, 
    {"-...."}, {"--..."}, {"---.."}, {"----."}, {"-----"}, 
    {"..--.."}, {"-..-."}, {".-.-.-"}, {"--..--"}, {"-...-"},
    {"---."}, {"----"}, {"..-.."}, {"..--"}, {".-.-"}
};
char symb[NUMBER_OF_ELEMENTS] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890?/.,=cseuj"; 


int enc_a_PinLast = LOW;              // Переменные для работы энкодера
int n = LOW;                          //
static long encoder_timer = 0;        // Таймер энкодера
boolean buttonActive = false;         // Флаг нажатия кнопки энкодера
boolean longPressActive = false;      // Флаг длительного нажатия кнопки энкодера

byte menu = 0;                        // Текущее состояние меню
long blink_timer = 0;                 // Таймер моргания
byte blink_flag = 0;                  // Флаг 
byte blink_state = 0;                 // Статус


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Wire.setClock(10000000);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(14, 0);
  display.print("UB3APP");
  display.setTextSize(1);
  display.setCursor(35, 24);
  display.print("CW Trainer");
  display.display();
  delay(3000);
  

  pinMode (enc_a_Pin, INPUT);         // sets encoder pins
  pinMode (enc_b_Pin, INPUT);
  pinMode (enc_btn_Pin, INPUT);
  digitalWrite(enc_btn_Pin, HIGH);    // Enable pullup resistor on encoder button

  
  pinMode(led_Pin, OUTPUT);           // sets the LED digital pin as output
  pinMode(LP_in, INPUT);              // sets Left Paddle digital pin as input
  pinMode(RP_in, INPUT);              // sets Right Paddle digital pin as input
  pinMode(ST_Pin, OUTPUT);            // Sets the Sidetone digital pin as output
  pinMode(key_Pin, OUTPUT);           // Sets the Keying Relay pin as output
  digitalWrite(led_Pin, LOW);         // turn the LED off
  digitalWrite(LP_in, HIGH);          // Enable pullup resistor on Left Paddle Input Pin
  digitalWrite(RP_in, HIGH);          // Enable pullup resistor on Right Paddle Input Pin


  EEPROM.get(0, so);
  if ( (so.ispeed == -1) or (so.ifreq == -1) ) {
    so.ispeed = 15;
    so.ifreq = 800;
    EEPROM.put(0, so);
  }

  ST_Freq = so.ifreq;
  key_speed = so.ispeed;

  keyerState = IDLE;

  display.setTextSize(1);
  display.clearDisplay();
  display.display();
  print_lcd_menu(true);
}

void loop() {
/*
 * WPM set
 */
  ditTime = 1200 / key_speed;
  

/*
 * Key state loop
 */
  switch (keyerState) {
    case IDLE:      // Wait for direct or latched paddle press
      if ((digitalRead(LP_in) == LOW) || (digitalRead(RP_in) == LOW) || (keyerControl & 0x03)) {
        update_PaddleLatch();
        keyerState = CHK_DIT;

        idle_first_flag = 0;
      }
      
      if ((digitalRead(LP_in) != LOW) && (digitalRead(RP_in) != LOW)) {
        if (idle_first_flag == 0) {
          ktimer_idle_dit = ditTime * 3;
          ktimer_idle_dah = ditTime * 7;
          idle_first_flag = 1;
        }

        if (idle_first_flag == 1) {
          ktimer_idle_dit += millis();
          ktimer_idle_dah += millis();
          idle_first_flag = 2;
        }

        if ( (millis() > ktimer_idle_dit) && idle_first_flag == 2 ) {
          Serial.print(code2char(ch));
          text1 += " ";
          text2 += code2char(ch);
          ch = "";
          print_lcd();
          idle_first_flag = 3;
        }

        if ( (millis() > ktimer_idle_dah) && idle_first_flag == 3 ) {
          Serial.print(" ");
          text1 += "  ";
          text2 += " ";
          print_lcd();
          idle_first_flag = 4;
        }

      }
      break;
    
    case CHK_DIT:      // See if the dit paddle was pressed
      if (keyerControl & DIT_L) {
        keyerControl |= DIT_PROC;
        ktimer = ditTime;
        keyerState = KEYED_PREP;
      }  else {
        keyerState = CHK_DAH;
      }
      break;

    case CHK_DAH:      // See if dah paddle was pressed
      if (keyerControl & DAH_L) {
        ktimer = ditTime*3;
        keyerState = KEYED_PREP;
      } else {
        keyerState = IDLE;
      }
      break;

    case KEYED_PREP:      // Assert key down, start timing, state shared for dit or dah
      digitalWrite(led_Pin, HIGH);         // Turn the LED on
      tone(ST_Pin, ST_Freq);               // Turn the Sidetone on
      digitalWrite(key_Pin, HIGH);         // Key the TX Relay
      
      if (keyerControl & DIT_L) {
        ch += '.'; 
        text1 += '.';
      };
      if (keyerControl & DAH_L) {
        ch += '-'; 
        text1 += '-';
      };
      
      ktimer += millis();                  // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L);    // clear both paddle latch bits
      keyerState = KEYED;                  // next state
      break;

    case KEYED:      // Wait for timer to expire
      if (millis() > ktimer) {  // are we at end of key down ?
        digitalWrite(led_Pin, LOW);      // Turn the LED off 
        noTone(ST_Pin);                  // Turn the Sidetone off
        digitalWrite(key_Pin, LOW);      // Turn the TX Relay off
        ktimer = millis() + ditTime;     // inter-element time
        keyerState = INTER_ELEMENT;      // next state
      } else { // Check to see if we're in Iambic B Mode
        if (key_mode == 1) update_PaddleLatch();           // early paddle latch in Iambic B mode
      }
      break;

    case INTER_ELEMENT:      // Insert time between dits/dahs
      update_PaddleLatch();               // latch paddle state
      if (millis() > ktimer) { // are we at end of inter-space ?
        if (keyerControl & DIT_PROC) {      // was it a dit or dah ?
          keyerControl &= ~(DIT_L + DIT_PROC);   // clear two bits
          keyerState = CHK_DAH;                  // dit done, check for dah
        } else {
          keyerControl &= ~(DAH_L);              // clear dah latch
          keyerState = IDLE;                     // go idle
        }
      }
      break;
  }


/*
 * Encoder rotation part
 */
  n = digitalRead(enc_a_Pin);
  if ((enc_a_PinLast == LOW) && (n == HIGH)) {
    if (digitalRead(enc_b_Pin) == LOW) {
      if (menu == 1) {
        key_speed--;
        if (key_speed < 5) key_speed = 5;
      }
      if (menu == 2) {
        ST_Freq -= 50;
        if (ST_Freq < 400) ST_Freq = 400;
      }

      if (menu != 0) {
        so.ispeed = key_speed;
        so.ifreq = ST_Freq;
        EEPROM.put(0, so);
      }
    } else {
      if (menu == 1) {
        key_speed++;
        if (key_speed > 40) key_speed = 40;
      }
      if (menu == 2) {
        ST_Freq += 50;
        if (ST_Freq > 1200) ST_Freq = 1200;
      }

      if (menu != 0) {
        so.ispeed = key_speed;
        so.ifreq = ST_Freq;
        EEPROM.put(0, so);
      }
    }
  }
  enc_a_PinLast = n;

  
/*
 * Encoder button part
 */
  if (digitalRead(enc_btn_Pin) == LOW) {
    if (buttonActive == false) {
      buttonActive = true;
      encoder_timer = millis();
    }
    if ((millis() - encoder_timer > longPressTime) && (longPressActive == false)) {
      longPressActive = true;
      text1 = "";
      text2 = "";
      print_lcd();
    }
  } else {
    if (buttonActive == true) {
      if (longPressActive == true) {
        longPressActive = false;
      } else {
        menu++;
        if (menu > 2) {
          menu = 0;
          print_lcd_menu(true);
        }
      }
      buttonActive = false;
    }
  }


/*
 * Blink timer
 */
  if ( blink_flag == 0 ) {
    blink_flag = 1;
    blink_timer = millis() + blink_time;
  }
  if ( (blink_flag == 1) && (millis() > blink_timer) ) {
    blink_flag = 0;
    if ( menu != 0 ) print_lcd_menu(true);
  }

  
}

void update_PaddleLatch() {
  if (digitalRead(RP_in) == LOW) keyerControl |= DIT_L;
  if (digitalRead(LP_in) == LOW) keyerControl |= DAH_L;
}

String code2char(String c) {
  String r = "#";
  for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
    if ( c == codecw[i] ) {
      r = symb[i];
      break;
    }
  }
  if ( r == "c" ) r = "Ch";
  if ( r == "s" ) r = "Sh";
  if ( r == "e" ) r = "Ee";
  if ( r == "u" ) r = "Ju";
  if ( r == "j" ) r = "Ja";
  return r;
}

void print_lcd() {
  if ( (text1 == text1_prev) && (text2 == text2_prev) ) return;

  int MAX_SYMB = 22;
  if ( text1.length() > MAX_SYMB) text1 = text1.substring(text1.length() - MAX_SYMB, text1.length());
  if ( text2.length() > MAX_SYMB) text2 = text2.substring(text2.length() - MAX_SYMB, text2.length());
  
  text1_prev = text1;
  text2_prev = text2;


  display.clearDisplay();

  print_lcd_menu(false);

  display.setCursor(0, 8);
  display.print(text1);

  display.setCursor(0, 24);
  display.print(text2);
  
  display.display();
}

void lcd_cll(int line = 1) {
  for (int y=0*line; y<=6*line; y++) {
    for (int x=0; x<127; x++) {
      display.drawPixel(x, y, BLACK);
      }
  }
}

void print_lcd_menu(boolean cls) {
  display.setCursor(0, 0);
  
  if ( menu == 0 ) {
    //display.print(cls_str);
    display.setCursor(0, 0);
    display.print("WPM: "+String(key_speed)+"    TONE: "+String(ST_Freq));
  }

  if ( menu == 1 ) {
    if (blink_state == 0) {
      blink_state = 1;
      lcd_cll();
      display.setCursor(0, 0);
      display.print("WPM:       TONE: "+String(ST_Freq));
    } else {
      blink_state = 0;
      lcd_cll();
      display.setCursor(0, 0);
      display.print("WPM: "+String(key_speed)+"    TONE: "+String(ST_Freq));
    }
  }

  if ( menu == 2 ) {
    if (blink_state == 0) {
      blink_state = 1;
      lcd_cll();
      display.setCursor(0, 0);
      display.print("WPM: "+String(key_speed)+"    TONE: ");
    } else {
      blink_state = 0;
      lcd_cll();
      display.setCursor(0, 0);
      display.print("WPM: "+String(key_speed)+"    TONE: "+String(ST_Freq));
    }
  }
  
  if (cls) display.display();
}
