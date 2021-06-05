// based on ColorTwinkles by Mark Kriegsman: https://gist.github.com/kriegsman/5408ecd397744ba0393e

#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
#define DENSITY            255

enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };

CRGB makeBrighter( const CRGB& color, fract8 howMuchBrighter)
{
  CRGB incrementalColor = color;
  incrementalColor.nscale8( howMuchBrighter);
  return color + incrementalColor;
}

CRGB makeDarker( const CRGB& color, fract8 howMuchDarker)
{
  CRGB newcolor = color;
  newcolor.nscale8( 255 - howMuchDarker);
  return newcolor;
}

// Compact implementation of
// the directionFlags array, using just one BIT of RAM
// per pixel.  This requires a bunch of bit wrangling,
// but conserves precious RAM.  The cost is a few
// cycles and about 100 bytes of flash program memory.
uint8_t  directionFlags[ (NUM_LEDS + 7) / 8];

bool getPixelDirection( uint16_t i)
{
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  andMask = 1 << bitNum;
  return (directionFlags[index] & andMask) != 0;
}

void setPixelDirection( uint16_t i, bool dir)
{
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  orMask = 1 << bitNum;
  uint8_t andMask = 255 - orMask;
  uint8_t value = directionFlags[index] & andMask;
  if ( dir ) {
    value += orMask;
  }
  directionFlags[index] = value;
}

void brightenOrDarkenEachPixel( fract8 fadeUpAmount, fract8 fadeDownAmount, LedInterval* interval)
{
  for ( uint16_t i = 0; i < interval->NUMLEDS; i++) {
    if ( getPixelDirection(interval->getPosition(i)) == GETTING_DARKER) {
      // This pixel is getting darker
      leds[interval->getPosition(i)] = makeDarker( leds[interval->getPosition(i)], fadeDownAmount);
    } else {
      // This pixel is getting brighter
      leds[interval->getPosition(i)] = makeBrighter( leds[interval->getPosition(i)], fadeUpAmount);
      // now check to see if we've maxxed out the brightness
      if ( leds[interval->getPosition(i)].r == 255 || leds[interval->getPosition(i)].g == 255 || leds[interval->getPosition(i)].b == 255) {
        // if so, turn around and start getting darker
        setPixelDirection(interval->getPosition(i), GETTING_DARKER);
      }
    }
  }
}

void colortwinkles(LedInterval* interval)
{
  EVERY_N_MILLIS(30)
  {
    // Make each pixel brighter or darker, depending on
    // its 'direction' flag.
    brightenOrDarkenEachPixel( FADE_IN_SPEED, FADE_OUT_SPEED, interval);
  
    // Now consider adding a new random twinkle
    if ( random8() < DENSITY ) {
      int pos = random16(interval->NUMLEDS);
      if ( !leds[interval->getPosition(pos)]) {
        leds[interval->getPosition(pos)] = ColorFromPalette( interval->gCurrentPalette, random8(), STARTING_BRIGHTNESS, NOBLEND);
        setPixelDirection(interval->getPosition(pos), GETTING_BRIGHTER);
      }
    }
  }
}

void cloudTwinkles(LedInterval* interval)
{
  interval->gCurrentPalette = CloudColors_p; // Blues and whites!
  colortwinkles(interval);
}

void rainbowTwinkles(LedInterval* interval)
{
  interval->gCurrentPalette = RainbowColors_p;
  colortwinkles(interval);
}

void snowTwinkles(LedInterval* interval)
{
  CRGB w(85, 85, 85), W(CRGB::White);

  interval->gCurrentPalette = CRGBPalette16( W, W, W, W, w, w, w, w, w, w, w, w, w, w, w, w );
  colortwinkles(interval);
}

void incandescentTwinkles(LedInterval* interval)
{
  CRGB l(0xE1A024);

  interval->gCurrentPalette = CRGBPalette16( l, l, l, l, l, l, l, l, l, l, l, l, l, l, l, l );
  colortwinkles(interval);
}

