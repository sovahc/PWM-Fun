#define FAN_FEEDBACK_PINS_C_COUNT 3
#define FAN_FEEDBACK_PINS_C { 16, 15, 14 }
#define FAN_FEEDBACK_PINS_D_COUNT 4
#define FAN_FEEDBACK_PINS_D { 7, 4, 5, 6 }

// LCD 0802 pins:
static const int LCD_RS = 2;
static const int LCD_E  = 1;
static const int LCD_D4 = 19;
static const int LCD_D5 = 0;
static const int LCD_D6 = 18;
static const int LCD_D7 = 17;

#include <LiquidCrystal.h>

LiquidCrystal lcd_move_me_to_class(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

class My_display
{
    static constexpr LiquidCrystal& lcd = lcd_move_me_to_class;

    static const byte celsius_character = 0;
    static const byte fan_character = 1;

  public:
    static void initialize()
    {
      delay(100); // Wait for display is powered up.
      lcd.begin(8, 2);

      byte celsius_character_mask[] PROGMEM = {
        B11100,
        B10100,
        B11100,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000
      };

      byte fan_character_mask[] PROGMEM = {
        B01100,
        B01100,
        B00101,
        B11011,
        B11011,
        B10100,
        B00110,
        B00110
      };

      lcd.createChar(celsius_character, celsius_character_mask);
      lcd.createChar(fan_character, fan_character_mask);

      /*
        // Custom characters for the progress bar
        uint8_t bars[] = {B10000, B11000, B11100, B11110, B11111};
        uint8_t character[8];
        for (uint8_t i = 0; i < 5; ++i) {
        for (uint8_t j = 0; j < 8; ++j) character[j] = bars[i];
        lcd.createChar(i, character);
        }*/

      lcd.setCursor(0, 0);
      lcd.print("Water   ");
      lcd.setCursor(0, 1);
      lcd.print("VHPWR   ");
    }

    static void print_temperature(int line, float t, bool none)
    {
      lcd.setCursor(0, line);

      if(none)
      { lcd.print("NONE   ");
      }
      else
      { lcd.print(t, 1);
        lcd.write(celsius_character);
        lcd.print("C ");
      }
    }

    static void print_fans_count(int count)
    {
      lcd.setCursor(7, 0);
      lcd.write(fan_character);
      lcd.setCursor(7, 1);
      lcd.print(count);
    }

    static void flash_led(int delay_ms, int count)
    {
      for (int i = 0; i < count; ++i)
      { digitalWrite(LED_BUILTIN, HIGH);
        delay(delay_ms);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delay_ms);
      }
    }

    static void display_error(const char* m1, const char* m2)
    { lcd.setCursor(0, 0);
      lcd.print(m1);
      lcd.setCursor(0, 1);
      lcd.print("        ");

      flash_led(50, 3);

      lcd.setCursor(0, 0);
      lcd.print("        ");
      lcd.setCursor(0, 1);
      lcd.print(m2);

      flash_led(50, 3);
    }
};
