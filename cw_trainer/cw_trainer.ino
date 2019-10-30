#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define ST_Pin 9        // Sidetone Output Pin on Pin 4
#define LP_in 7         // Left Paddle Input on Pin 7
#define RP_in 5         // Right Paddle Input on Pin 5
#define led_Pin 13      // LED on Pin 13
//#define Mode_A_Pin  3   // Mode Select Switch Side A
//#define Mode_B_Pin  2   // Mode Select Switch Side B
#define key_Pin 6       // Transmitter Relay Key Pin
//#define Speed_Pin 0     // Speed Control Pot on A0
//#define ST_Freq 800     // Set the Sidetone Frequency to 600 Hz

int ST_Freq;

int key_speed;          // variable for keying speed / скорость wpm
int key_mode;           // variable for keying mode
int last_mode;          // variable to detect keying mode change

struct SettingsObj {
  int ifreq;
  int ispeed; 
};

SettingsObj so;

unsigned long ditTime;  // Number of milliseconds per dit
char keyerState;
char keyerControl;

static long ktimer;

static long ktimer_idle_dit;
static long ktimer_idle_dah;
int idle_first_flag = 4;

String ch = "";

String text1 = "";
String text2 = "";

String text1_prev = "x";
String text2_prev = "x";

const int NUMBER_OF_ELEMENTS = 46;
const int MAX_SIZE = 7;
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


//  State Machine Defines
enum KSTYPE {IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT};

//  keyerControl bit definitions
#define     DIT_L      0x01     // Dit latch
#define     DAH_L      0x02     // Dah latch
#define     DIT_PROC   0x04     // Dit is being processed
//#define     PDLSWAP    0x08     // 0 for normal, 1 for swap
//#define     IAMBICB    0x10     // 0 for Iambic A, 1 for Iambic B
//#define     ULTIMATIC  0x20     // 1 for ultimatic
//#define     STRAIGHT   0x80     // 1 for straight key mode



int val;
int encoder0PinSW = 2;
int encoder0PinA = 3;
int encoder0PinB = 4;
//int encoder0Pos = 0;
int encoder0PinALast = LOW;
int n = LOW;
static long encoder_timer = 0;
long longPressTime = 500;
boolean buttonActive = false;
boolean longPressActive = false;

byte menu = 0;
long blink_timer = 0;
int blink_time = 500;
byte blink_flag = 0;
byte blink_state = 0;

void setup() {
  Serial.begin(115200);
  //Serial.println("UB3APP");

  Wire.setClock(10000000);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  pinMode (encoder0PinA, INPUT);
  pinMode (encoder0PinB, INPUT);
  pinMode (encoder0PinSW, INPUT);
  digitalWrite(encoder0PinSW, HIGH);

  
  pinMode(led_Pin, OUTPUT);         // sets the LED digital pin as output
  pinMode(LP_in, INPUT);            // sets Left Paddle digital pin as input
  pinMode(RP_in, INPUT);            // sets Right Paddle digital pin as input
  pinMode(ST_Pin, OUTPUT);          // Sets the Sidetone digital pin as output
  pinMode(key_Pin, OUTPUT);         // Sets the Keying Relay pin as output
  //pinMode(Mode_A_Pin, INPUT);       // sets Iambic Mode Switch Side A as input
  //pinMode(Mode_B_Pin, INPUT);       // sets Iambic Mode Switch Side B as input
  digitalWrite(led_Pin, LOW);       // turn the LED off
  digitalWrite(LP_in, HIGH);        // Enable pullup resistor on Left Paddle Input Pin
  digitalWrite(RP_in, HIGH);        // Enable pullup resistor on Right Paddle Input Pin
  //digitalWrite(Mode_A_Pin, HIGH);   // Enable pullup resistor on Mode Switch Side A Input Pin
  //digitalWrite(Mode_B_Pin, HIGH);   // Enable pullup resistor on Mode Switch Side B Input Pin


  EEPROM.get(0, so);
  if ( (so.ispeed == -1) or (so.ifreq == -1) ) {
    so.ispeed = 15;
    so.ifreq = 800;
    EEPROM.put(0, so);
  }

  ST_Freq = so.ifreq;
  key_speed = so.ispeed;

  keyerState = IDLE;

  //print_lcd();
  print_lcd_menu(true);
}

void loop() {
  // Read the Mode Switch and set mode 
  //
  // Key Mode 0 = Iambic Mode A
  // Key Mode 1 = Iambic Mode B
  // Key Mode 2 = Straight Key
  //
  /*
  if (digitalRead(Mode_A_Pin) == LOW)   // Set Iambic Mode A
  {
    key_mode = 0;
    //text3 = "Mode: Iambic-A";
  }
  if (digitalRead(Mode_B_Pin) == LOW)  // Set Iambic Mode B
  {
    key_mode = 1;
    //text3 = "Mode: Iambic-B";
  }
  if (digitalRead(Mode_A_Pin) == HIGH && digitalRead(Mode_B_Pin) == HIGH)  // Set Straight Key
  {
    key_mode = 2;
    //text3 = "Mode: Straight";
  }
  if (key_mode != last_mode)
  {
    last_mode = key_mode;
    //updatelcd();
  }
  */
  ditTime = 1200 / key_speed;
  
  key_mode = 0;

  if (key_mode != 2) {
    // Basic Iambic Keyer
    // keyerControl contains processing flags and keyer mode bits
    // Supports Iambic A and B
    // State machine based, uses calls to millis() for timing.
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
          //print_lcd();
        };
        if (keyerControl & DAH_L) {
          ch += '-'; 
          text1 += '-';
          //print_lcd();
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
  }



  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(encoder0PinB) == LOW) {
      //encoder0Pos--;
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
      //encoder0Pos++;
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
    //Serial.print (encoder0Pos);
    //Serial.print ("/");
  }
  encoder0PinALast = n;

/*
  if (digitalRead(encoder0PinSW) == LOW) {
    Serial.print("encoder0 SW");
  }
*/

  if (digitalRead(encoder0PinSW) == LOW) {
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
        //
        //Serial.println(menu);
      }
      buttonActive = false;
    }
  }


  if ( blink_flag == 0 ) {
    blink_flag = 1;
    blink_timer = millis() + blink_time;
    //Serial.println("blink 0");
    //if ( menu != 0 ) print_lcd_menu(true);
  }
  if ( (blink_flag == 1) && (millis() > blink_timer) ) {
    blink_flag = 0;
    if ( menu != 0 ) print_lcd_menu(true);
    //blink_timer = 0;
    //Serial.println("blink 1");
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
  //display.setCursor(0, 0);
  //display.print("                    ");
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
  //Serial.println("menu - "+String(blink_state));
}
