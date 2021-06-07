/*
   ESP8266 + FastLED + IR Remote: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2016 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

uint8_t power = 1;
uint8_t brightness = brightnessMap[brightnessIndex];
uint8_t addIntervalField = 0;
//String setPower(String value) {
//  power = value.toInt();
//  if(power < 0) power = 0;
//  else if (power > 1) power = 1;
//  return String(power);
//}

String getPower() {
  return String(power);
}

String getAddInterval() {
  return String(addIntervalField);
}

//String setBrightness(String value) {
//  brightness = value.toInt();
//  if(brightness < 0) brightness = 0;
//  else if (brightness > 255) brightness = 255;
//  return String(brightness);
//}

String getBrightness() {
  if(changeFullStrip) {
    return String(fullStrip.brightness);
  } else {
    return String(intervals[currentChangingInterval].brightness);
  }
}

String getPattern() {
  if(changeFullStrip) {
    return String(fullStrip.getPatternIndex());
  } else {
    return String(intervals[currentChangingInterval].getPatternIndex());
  }
}

String getPatterns() {
  String json = "";

  for (uint8_t i = 0; i < patternCount; i++) {
    json += "\"" + patterns[i].name + "\"";
    if (i < patternCount - 1)
      json += ",";
  }

  return json;
}

String getPalette() {
  if(changeFullStrip) {
    return String(fullStrip.getPaletteIndex());
  } else {
    return String(intervals[currentChangingInterval].getPaletteIndex());
  }
}

String getPalettes() {
  String json = "";

  for (uint8_t i = 0; i < paletteCount; i++) {
    json += "\"" + paletteNames[i] + "\"";
    if (i < paletteCount - 1)
      json += ",";
  }

  return json;
}

String getAutoplay() {
  return String(autoplay);
}

String getAutoplayDuration() {
  return String(autoplayDuration);
}

String getSolidColor() {
  return String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b);
}

String getCooling() {
  return String(cooling);
}

String getSparking() {
  return String(sparking);
}

String getSpeed() {
  return String(speed);
}

String getTwinkleSpeed() {
  if(changeFullStrip) {
    return String(fullStrip.twinkleSpeed);
  } else {
    return String(intervals[currentChangingInterval].twinkleSpeed);
  }
}

String getTwinkleDensity() {
  if(changeFullStrip) {
    return String(fullStrip.twinkleDensity);
  } else {
    return String(intervals[currentChangingInterval].twinkleDensity);
  }
}

String getIntervals() {
  String json = "";
  for (uint8_t i = 0; i < intervalsAmount; i++) {
    json += "\"" + String(i) + " - " + patterns[intervals.at(i).getPatternIndex()].name + " (" + paletteNames[intervals.at(i).getPaletteIndex()]+ + ")\"";
    if (i < intervalsAmount - 1)
      json += ",";
  }
  return json;
}

String getInterval() {
  return String(currentChangingInterval);
}

String getAddIntervalText() {
  String str = "\"Add Interval\", \"Delete Interval\", \"Sync Intervals\"";
  return str;
}

String getFullStrip() {
  return String(changeFullStrip);
}
String getIntervalStart() {
  if(changeFullStrip) {
    return String(fullStrip.start + 1);
  } else {
    return String(intervals[currentChangingInterval].start + 1);
  }
}

String getIntervalEnd() {
  if(changeFullStrip) {
    return String(fullStrip.end);
  } else {
    return String(intervals[currentChangingInterval].end);
  }
}
String getNull() {
  return "\"\"";
}
String getChangeFullStrip() {
  return ((changeFullStrip == 1) ? "true" : "false");
}

String getChangeAllIntervals() {
  return String(changeAllIntervals);
}
String getInvertDirection() {
  if(changeFullStrip) {
    return String(fullStrip.inverted);
  } else {
    return String(intervals[currentChangingInterval].inverted);
  }
}
String getDivideLastInterval() {
  return String(divideLastInterval);
}

String getAmountDivisions() {
  if(changeFullStrip) {
    return String(fullStrip.divisions);
  } else {
    return String(intervals[currentChangingInterval].divisions);
  }
}

String getUseSolidColor() {
  if(changeFullStrip) {
    return String(fullStrip.useSolidColor);
  } else {
    return String(intervals[currentChangingInterval].useSolidColor);
  }
}

FieldList fields = {
  { "interval", "Intervals", SectionFieldType },
  { "fullStrip", "Entire Strip/Intervals", BooleanFieldType, 0, 1, getFullStrip, getNull, true },
  { "changeAllIntervals", "Change All Intervals", BooleanFieldType, 0, 1, getChangeAllIntervals, getNull, true, getChangeFullStrip },
  { "interval", "Interval", SelectFieldType, 0, intervalsAmount, getInterval, getIntervals, true, getChangeFullStrip },
  { "intervalStart", "Interval Start", NumberFieldType, 1, NUM_LEDS, getIntervalStart, getNull, false },
  { "intervalEnd", "Interval End", NumberFieldType, 1, NUM_LEDS, getIntervalEnd, getNull, false },
  { "intervalOptions", "Interval Options", ButtonFieldType, 0, 1, getNull, getAddIntervalText, true, getChangeFullStrip },
  { "divideLastInterval", "Divide last interval when adding?", BooleanFieldType, 0, 1, getDivideLastInterval, getNull, true, getChangeFullStrip },
  { "setting", "Settings", SectionFieldType },
  { "power", "Power", BooleanFieldType, 0, 1, getPower },
  { "brightness", "Brightness", NumberFieldType, 1, 255, getBrightness },
  { "pattern", "Pattern", SelectFieldType, 0, patternCount, getPattern, getPatterns },
  { "invertDirection", "Invert Direction", BooleanFieldType, 0, 1, getInvertDirection },
  { "useSolidColor", "Use Solid Color as Palette", BooleanFieldType, 0, 1, getUseSolidColor },
  { "palette", "Palette", SelectFieldType, 0, paletteCount, getPalette, getPalettes },
  { "speed", "Speed", NumberFieldType, 1, 255, getSpeed },
  { "autoplay", "Autoplay", SectionFieldType },
  { "autoplay", "Autoplay", BooleanFieldType, 0, 1, getAutoplay },
  { "autoplayDuration", "Autoplay Duration", NumberFieldType, 0, 255, getAutoplayDuration },
  { "solidColor", "Solid Color", SectionFieldType },
  { "solidColor", "Color", ColorFieldType, 0, 255, getSolidColor },
  { "fire", "Fire & Water", SectionFieldType },
  { "cooling", "Cooling", NumberFieldType, 0, 255, getCooling },
  { "sparking", "Sparking", NumberFieldType, 0, 255, getSparking },
  { "twinkles", "Twinkles", SectionFieldType },
  { "twinkleSpeed", "Twinkle Speed", NumberFieldType, 0, 8, getTwinkleSpeed },
  { "twinkleDensity", "Twinkle Density", NumberFieldType, 0, 255, getTwinkleDensity },
  { "amountDivisions", "Amount Divisions", NumberFieldType, 1, 30, getAmountDivisions },
};

uint8_t fieldCount = ARRAY_SIZE(fields);
