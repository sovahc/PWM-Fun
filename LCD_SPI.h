#include <Adafruit_GFX.h> // graphics library
#include <Adafruit_ST7789.h> // library for this display
#include <SPI.h>

#define FAN_FEEDBACK_PINS_C_COUNT 4
#define FAN_FEEDBACK_PINS_D_COUNT 3
#define FAN_FEEDBACK_PINS_C { 15, 14, 16, 17 }
#define FAN_FEEDBACK_PINS_D { 5, 6, 7 }

class TFT
{
  Adafruit_ST7789 tft = Adafruit_ST7789(0, 19, 18); // cs, dc, rs
public:
  static const int W = 320;
  static const int H = 170;

  static const uint16_t WHITE  = ST77XX_WHITE;
  static const uint16_t BLACK  = ST77XX_BLACK;
  static const uint16_t RED    = ST77XX_RED;
  static const uint16_t YELLOW = ST77XX_YELLOW;
  // ______________________________RRRRRGGGGGGBBBBB // 565
  static const uint16_t GRAY      = 0b0111101111101111; 
  static const uint16_t DARK_GRAY = 0b0000100001100001;
  static const uint16_t BLUE  = ST77XX_BLUE;

  void initialize()
  { tft.init(H, W, SPI_MODE0);
    //tft.setSPISpeed(800000); // SLOW
    tft.setRotation(3);
    //tft.fillScreen(DARK_GRAY);
    tft.fillScreen(BLACK);
    tft.setTextWrap(false);
  }

  void fill_rectangle(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
  { tft.fillRect(x, y, w, h, color);
  }

  void draw_text(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint16_t background, const char* text)
  { tft.setTextSize(1);
    tft.setTextColor(color, background);
    tft.setCursor(x, y);
    tft.print(text);
  }

  #define MAKE_RGB565(r, g, b) ((r << 11) | (g << 5) | (b))

  uint16_t blend_rgb565(uint16_t a, uint16_t b, uint8_t Alpha)
  {
    const uint8_t invAlpha = 255 - Alpha;

    uint16_t A_r = a >> 11;
    uint16_t A_g = (a >> 5) & 0x3f;
    uint16_t A_b = a & 0x1f;

    uint16_t B_r = b >> 11;
    uint16_t B_g = (b >> 5) & 0x3f;
    uint16_t B_b = b & 0x1f;

    uint32_t C_r = (A_r * invAlpha + B_r * Alpha) / 255;
    uint32_t C_g = (A_g * invAlpha + B_g * Alpha) / 255;
    uint32_t C_b = (A_b * invAlpha + B_b * Alpha) / 255;

    return MAKE_RGB565(C_r, C_g, C_b);
  }

  void bublic(int16_t x0, int16_t y0, int16_t r1, int16_t r2, uint16_t color, uint16_t background)
  { 
    // not optimized
    auto A = r1*r1;
    auto B = (r1-1)*(r1-1);
    auto C = r2*r2;
    auto D = (r2-1)*(r2-1);

    if(r2 < 0)
    { C = -C;
      D = -D;
    }

    for(int16_t y = -r1; y < r1; ++y)
      for(int16_t x = -r1; x < r1; ++x)
      { 
        uint16_t c;
        auto d = x*x + y*y;
        
        if(d > A) c = background;
        else if(d > B) c = blend_rgb565(background, color, map(d, B, A, 255, 0));
        else if(d > C) c = color;
        else if(d > D) c = blend_rgb565(background, color, map(d, D, C, 0, 255));
        else c = background;

        tft.drawPixel(x0+x, y0+y, c);
      }
  }
} tft;

static const uint8_t D0_9[] = // Rotated CCW
{
  0b11111, // 0
  0b10001,
  0b11111,

  0b11111, // 1
  0b00000,
  0b00000,

  0b11101, // 2
  0b10101,
  0b10111,

  0b11111, // 3
  0b10101,
  0b10101,

  0b11111, // 4
  0b00100,
  0b11100,

  0b10111, // 5
  0b10101,
  0b11101,

  0b10111, // 6
  0b10101,
  0b11111,

  0b11111, // 7
  0b10000,
  0b10000,

  0b11111, // 8
  0b10101,
  0b11111,

  0b11111, // 9
  0b10101,
  0b11101,
};

class Big_digits
{
  static const int w1 = 2;  
  static const int h1 = 6;
  static const uint16_t color = 0xFFff;
  static const uint16_t background = 0;

  static int draw_digit(char c, int16_t x, int16_t y, int16_t w, int16_t h, bool draw_holes)
  { 
    uint8_t b1, b2, b3;

    if(c >= '0' && c <= '9')
    { 
      int offset = (c - '0') * 3;

      b1 = D0_9[offset+2];
      b2 = D0_9[offset+1];
      b3 = D0_9[offset+0];
    }
    else if(c == ' ')
    { b1 = b2 = b3 = 0;      
    }
    else if(c == 'C')
    { b3 = 0b10001;
      b2 = 0b10001;
      b1 = 0b11111;
    }
    else if(c == '-')
    { b3 = 0b00100;
      b2 = 0b00100;
      b1 = 0b00100;
    }
    else if(c == '\'')
    {
#if false
        // squared °
        w /= 2;
        if(draw_holes) tft.fill_rectangle(x, y+h/2, w, h/2, background);
        h /= 2;
        b1 = 0b11100;
        b2 = 0b10100;
        b3 = 0b11100;
#else
        // round antialiased °
        w /= 2;
        if(draw_holes) tft.fill_rectangle(x, y+w, w, h-w, background);
        tft.bublic(x+w/2, y+w/2, w/2, w/2-w1, color, background);
        return w;
#endif
    }
    else if(c == '.')
    { 
#if true
      // square dot
      auto r = h1;
      if(draw_holes) tft.fill_rectangle(x, y, r, h-r, background);
      tft.fill_rectangle(x, y+h-r, r, r, color);
      return r;
#else
      // round dot
      auto r = h1;
      if(draw_holes) tft.fill_rectangle(x, y, r, h-r, background);
      tft.bublic(x+r/2, y+h-r/2, r/2, -1, color, background);
      return r;
#endif
    }

    int16_t w2 = w - w1 - w1;
    int16_t h2 = h - h1 - h1 - h1;
    h2 /= 2;

    for(int j = 0; j < 5; ++j)
    { int bit = 4-j;
      bool s1 = (b1 >> bit) & 1;
      bool s2 = (b2 >> bit) & 1;
      bool s3 = (b3 >> bit) & 1;

      int16_t h = j & 1 ? h2 : h1;

      tft.fill_rectangle(x,       y, w1, h, s1 ? color : background);
      if((j != 1 && j != 3) || draw_holes) //
      tft.fill_rectangle(x+w1,    y, w2, h, s2 ? color : background);
      tft.fill_rectangle(x+w1+w2, y, w1, h, s3 ? color : background);

      y += h;
    }

    return w;
  }
public:
  static void draw(int x, int y, int w, int h, int wspace, char* text, bool draw_holes)
  {
    for(int i = 0; text[i]; ++i)
    { x += draw_digit(text[i], x, y, w, h, draw_holes);
      if(draw_holes) tft.fill_rectangle(x, y, wspace, h, Big_digits::background);
      x += wspace;
    }
  }
};

class My_display
{
  static const int W = 320;
  static const int H = 170;

  static char tmp[0x20];

  public:
    static void initialize()
    {
        tft.initialize();
    }

    static void print_temperature(int line, float t, bool none)
    {
      const int wspace = 10;
      const int hspace = 32;
      const int w = 32;
      const int h = 64;
      int x = 0;
      int y = line * (h + hspace);

      if(t >= -90 && t < 90)
      { dtostrf(t, 5, 1, tmp);
        strcat(tmp, "'C");
        Big_digits::draw(x, y, w, h, wspace, tmp, false);
      }
      //else
      //  strcpy(tmp, "---.-");
    }

    static void print_fans_count(int count)
    {
      int R = 32;
      int x = TFT::W - 32;
      tft.draw_text(x, 0, R, R, TFT::WHITE, TFT::BLACK, "FANS");
      sprintf(tmp, "%d", count);
      Big_digits::draw(x, R, R/2, R, 0, tmp, true);
    }

    static void display_error(const char* m1, const char* m2)
    { 
      tft.draw_text(0, TFT::H-32, TFT::W, 16, TFT::YELLOW, TFT::RED, m1);
      tft.draw_text(0, TFT::H-16, TFT::W, 16, TFT::YELLOW, TFT::RED, m2);
    }
};

char My_display::tmp[] = "";
