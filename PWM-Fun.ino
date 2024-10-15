// This is Atmega328 based PWM fan controller for water cooled PC.

const float thermistor2_alert = 50.0f; // Celisus

#define WS2812_STRIP_CONNECTED false

#if WS2812_STRIP_CONNECTED
  #define TLED_ORDER ORDER_GRB
  #define TLED_CHIP LED_WS2812
  #include "tinyLED.h" // microLED library
tinyLED<0> strip; // port B pin 0 (Arduino pin 8)

void fill_led(uint8_t r, uint8_t g, uint8_t b)
{ for(int i = 0; i < 8; ++i)
    strip.sendRGB(r, g, b);  
}
#else
void fill_led(uint8_t r, uint8_t g, uint8_t b) {}
#endif

const char* error_message = nullptr;

#if true
  #include "LCD_SPI.h"
#else
  #include "LCD_0802.h"
#endif

// PWM pins (cannot be changed)
static const int PIN_PWM_1A = 9;
static const int PIN_PWM_1B = 10;
static const int PIN_PWM_2 = 3;
//

#define ARRAY_SIZE(a) ( sizeof(a)/sizeof(a[0]) )
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

struct Fan_curve_point
{ float temperature;
  float pwm;
};

class Fan_curve
{
    Fan_curve_point curve[10];
    int count = 0;

    const char* verify_curve()
    {
      for (int i = 0; i < count; ++i)
      { Fan_curve_point& p = curve[i];

        // Values must be in valid range.
        //if (p.temperature < 0 || p.temperature > 100) return "verify1";
        if (p.pwm < 0 || p.pwm > 1) return "verify2";

        if (i < 1) continue;

        Fan_curve_point& pp = curve[i - 1];

        // The points should be given in ascending order.
        if (p.temperature <= pp.temperature) return "verify3"; // "<=" - avoid division by zero
        if (p.pwm < pp.pwm) return "verify4";
      }
      return nullptr; // no error
    }

  public:
    void add_point(float temperature, float pwm)
    {
      if (count >= ARRAY_SIZE(curve))
      { error_message = "add_point";
        return;
      }

      curve[count].temperature = temperature;
      curve[count].pwm = pwm;
      ++count;

      // Automatic curve verification
      if(error_message == nullptr)
      { error_message = verify_curve();
      }
    }

    float calculate_pwm(float temperature)
    {
      if (count <= 0) return 1.0; // Full fan speed if no points defined.

      for(int i = 0; i < count-1; ++i)
      {
        if(temperature >= curve[i].temperature &&
          temperature <= curve[i+1].temperature) // in range
        {
          // interpolate

          Fan_curve_point& a = curve[i];
          Fan_curve_point& b = curve[i+1];
          float interpolation_factor = (temperature - a.temperature) / (b.temperature - a.temperature);
          
          return a.pwm + (b.pwm - a.pwm) * interpolation_factor;
        }
      }

      error_message = "no range";
      return 1.0;
    }
};

Fan_curve fan_curve;

void set_fan_curve()
{ // The minimum duty cycle is 20% according to Intel. Noctua fans can operate from 0%

#if false // Curve for testing the device
  fan_curve.add_point(-300, 0); // Don't remove this point
  fan_curve.add_point(27, 0);
  fan_curve.add_point(27.01, 0.9);
  fan_curve.add_point(300.0, 1.0); // Don't remove this point
#else
  fan_curve.add_point(-300, 0); // Don't remove this point
  fan_curve.add_point(30, 0);
  fan_curve.add_point(30.01, 0.2);
  fan_curve.add_point(60.0, 1.0);
  fan_curve.add_point(300.0, 1.0); // Don't remove this point
#endif
}

class Pwm_output_1
{
    static inline float clamp_01(float f)
    { if (f < 0) return 0;
      if (f > 1) return 1;
      return f;
    }

  public:
    static void initialize()
    {
      // https://fdossena.com/?p=ArduinoFanControl/i.md

      // Enable outputs for Timer 1
      pinMode(PIN_PWM_1A, OUTPUT);
      pinMode(PIN_PWM_1B, OUTPUT);

      // Set PWM frequency to about 25khz on pins 9,10 (timer 1 mode 10, no prescale, count to 320)
      TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
      TCCR1B = (1 << CS10) | (1 << WGM13);
      ICR1 = 320;
      OCR1A = 0;
      OCR1B = 0;
    }

    static void set_pin9_duty(float f)
    { OCR1A = (uint16_t)(320 * clamp_01(f));
    }

    static void set_pin10_duty(float f)
    { OCR1B = (uint16_t)(320 * clamp_01(f));
    }
};

class Thermistor
{
    // Values from the datasheet
    static const int R_25 = 10000; // Ω
    static const int B_value = 3977; // K
    //

  public:
    static float calculate_temperature(float analog)
    {
      const int VCC = 5;
      const int SERIAL_R = 10000; // thermistor serial resistor in Ohms
      const float T0 = 25 + 273.15; // thermistor T0 in Kelvins

      float thermistor_V = (5.00 / 1023.00) * analog;
      float resistor_V = VCC - thermistor_V;
      float thermistor_R = thermistor_V / (resistor_V / SERIAL_R);

      float ln = log(thermistor_R / R_25);
      float temperature = (1 / ((ln / B_value) + (1 / T0)));

      temperature -= 273.15;// to Celsius
      return temperature;
    }
};

class Fan
{
    int counter;
    unsigned long last_feedback_time;
    unsigned long base_feedback_time;

  public:
    int rpm;
    unsigned long total_feedback;

    void feedback() // called from interrupt
    {
      ++counter;
      ++total_feedback;
      last_feedback_time = millis();
    }

    void recalculate_rpm()
    {
      auto delta = last_feedback_time - base_feedback_time;

      rpm = counter * 60000 / 4 / delta; // two pulses per revolution * rise and fall = 4

      base_feedback_time = last_feedback_time;
      counter = 0;
    }
};

class Fans_feedback
{
    const int PORT_C_PIN0 = 14;

    const byte feedback_pins_C[FAN_FEEDBACK_PINS_C_COUNT] = FAN_FEEDBACK_PINS_C;
    const byte feedback_pins_D[FAN_FEEDBACK_PINS_D_COUNT] = FAN_FEEDBACK_PINS_D;

    static const int FANS_COUNT = ARRAY_SIZE(feedback_pins_C) + ARRAY_SIZE(feedback_pins_D);

    bool pin_state_C[ ARRAY_SIZE(feedback_pins_C) ];
    bool pin_state_D[ ARRAY_SIZE(feedback_pins_D) ];

  public:
    Fan fans[FANS_COUNT];

    void recalculate_rpm()
    {
      for (int i = 0; i < ARRAY_SIZE(fans); ++i)
      {
        fans[i].recalculate_rpm();
      }
    }

    inline void isr_port_c() // called from interrupt
    {
      for (int i = 0; i < ARRAY_SIZE(feedback_pins_C); ++i)
      { const int mask = 1 << (feedback_pins_C[i] - PORT_C_PIN0);
        bool b = PINC & mask;
        if (pin_state_C[i] != b)
        { pin_state_C[i] = b;

          fans[i].feedback();
        }
      }
    }

    inline void isr_port_d() // called from interrupt
    {
      for (int i = 0; i < ARRAY_SIZE(feedback_pins_D); ++i)
      { const int mask = 1 << feedback_pins_D[i];
        bool b = PIND & mask;
        if (pin_state_D[i] != b)
        { pin_state_D[i] = b;

          fans[i + ARRAY_SIZE(feedback_pins_C)].feedback();
        }
      }
    }

    void initialize()
    {
      // Activate PCINT

      for (auto pin : feedback_pins_C)
      { pinMode(pin, INPUT_PULLUP);
        PCMSK1 |= 1 << (pin - PORT_C_PIN0);
      }

      for (auto pin : feedback_pins_D)
      { pinMode(pin, INPUT_PULLUP);
        PCMSK2 |= 1 << pin;
      }

      PCICR |= B00000010; // activate the interrupts of the PC port
      PCICR |= B00000100; // activate the interrupts of the PD port
    }

} fans_feedback;

// Interrupts cannot be used inside a class, so we redirect them.
ISR (PCINT2_vect)
{ fans_feedback.isr_port_d();  // PORTD (Arduino pins: D0 to D7)
}
ISR (PCINT1_vect)
{ fans_feedback.isr_port_c();  // PORTC (Arduino pins: D14 to D22)
}

void setup()
{
  My_display::initialize(); // Display initial message

  Pwm_output_1::initialize();

  // Initially start the fans for a short time.
  Pwm_output_1::set_pin9_duty(0.5f);
  Pwm_output_1::set_pin10_duty(0.5f);

  fans_feedback.initialize();

  delay(500);

  //error_message = "TEST_" STRINGIZE(__LINE__); // Error message test

  set_fan_curve();

  short_beep();
}

void short_beep()
{
  tone(PIN_PWM_2, 1000);
  delay(200);
  noTone(PIN_PWM_2);
}

void loop()
{
  if (error_message != nullptr)
  {
    // If an error occurs, turn on all fans to full speed.
    Pwm_output_1::set_pin9_duty(1);
    Pwm_output_1::set_pin10_duty(1);

    My_display::display_error("ERROR", error_message);

    digitalWrite(LED_BUILTIN, HIGH);
    fill_led(100,100,0);
    short_beep();
    digitalWrite(LED_BUILTIN, LOW);
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);

    // Averaging thermistors samples to eliminate noise
    const int num_samples = 500;

    float average1 = 0;
    float average2 = 0;

    for (int i = 0; i < num_samples; ++i)
    { average1 += analogRead(A6); // thermistor pin number
      average2 += analogRead(A7); // thermistor pin number
      
      delay(1);
    }
    average1 /= num_samples;
    average2 /= num_samples;

    float t1 = Thermistor::calculate_temperature(average1);
    float t2 = Thermistor::calculate_temperature(average2);

    My_display::print_temperature(0, t1, average1 < 10);
    My_display::print_temperature(1, t2, average2 < 10);

    if(t2 < thermistor2_alert)
    { 
      float pwm = fan_curve.calculate_pwm(t1);

      Pwm_output_1::set_pin9_duty(pwm);
      Pwm_output_1::set_pin10_duty(pwm);
    }
    else
    { Pwm_output_1::set_pin9_duty(1);
      Pwm_output_1::set_pin10_duty(1);
      fill_led(100,0,0);
      short_beep();
    }

    fans_feedback.recalculate_rpm();
  
    int running_fans = 0;
    for(auto fan : fans_feedback.fans)
    { if(fan.rpm > 0) ++running_fans;
    }

    My_display::print_fans_count(running_fans);

    //auto rpm0 = fans_feedback.fans[0].rpm;

    fill_led(1,1,2);
  }
}
