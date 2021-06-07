class LedInterval {
    public:
        uint16_t index;
        uint16_t paletteIndex;
        uint16_t patternIndex;

        uint16_t start;
        uint16_t end;

        uint16_t sPseudotime = 0;
        uint16_t sLastMillis = 0;
        uint16_t sHue16 = 0;

        uint16_t NUMLEDS;
        uint16_t totalNUMLEDS;

        bool hasTwoStrips;
        uint16_t* pos;
        uint16_t* prevpos;
        /* ------ Start End Join ------ */
        uint8_t divisions = 1;
        /* ---------------------------- */
        uint16_t* helper;
        uint16_t amountPerDivision;
        uint8_t cooling = 49;
        uint8_t sparking = 60;
        /* ------ HeatMap ------ */
        byte* heat;
        /* --------------------- */

        uint16_t speed = 30;
        uint8_t brightness = 255;
        
        uint8_t autoplay = 0;
        uint8_t autoplayDuration = 10;
        unsigned long autoPlayTimeout = millis() + (autoplayDuration * 1000);
        uint8_t twinkleDensity = 20;
        uint8_t twinkleSpeed = 4;
        bool clear = false;
        bool inverted = false;
        bool useSolidColor = false;
        CRGBPalette16 gTargetPalette;
        CRGBPalette16 gCurrentPalette;
        CRGBPalette16 twinkleFoxPalette;
        CRGB solidColor = CRGB::Blue;
        /* ------ Juggle ------ */
        uint8_t lastSecond =  99;
        /* -------------------- */
        /* ------ Fireworks ------ */
        bool launching = true;
        LedInterval() {};
        LedInterval(uint16_t _start, uint16_t _end, uint16_t _NUMLEDS) {
            Serial.println("Creating new LedInterval..");
            start = _start;
            end = _end;
            clear = false;
            totalNUMLEDS = _NUMLEDS;
            paletteIndex = 0;
            patternIndex = 0;
            calculateAmount();
            setDivisions(1); 
            autoplay = 0;
            autoplayDuration = 10;
            Serial.print("start: ");
            Serial.print(_start);
            Serial.print(" - end: ");
            Serial.print(_end);
            Serial.print(" - NUMLEDS: ");
            Serial.print(NUMLEDS);
            Serial.println(" }");
            gCurrentPalette = CRGBPalette16(CRGB::Black);
            gTargetPalette = CRGBPalette16(CRGB::Black);
            heat = new byte[NUMLEDS];
        };
        LedInterval(uint16_t _start, uint16_t _end, uint16_t _NUMLEDS, uint16_t _paletteIndex, uint16_t _patternIndex){
            start = _start;
            end = _end;
            clear = false;
            totalNUMLEDS = _NUMLEDS;
            autoplay = 0;
            autoplayDuration = 10;
            calculateAmount();
            setDivisions(1);
            paletteIndex = _paletteIndex;
            patternIndex = patternIndex;
            gCurrentPalette = CRGBPalette16(CRGB::Black);
            gTargetPalette = CRGBPalette16(CRGB::Black);
            heat = new byte[NUMLEDS];
        };
        void set_gPalettes(CRGBPalette16 _gCurrentPalette, CRGBPalette16 _gTargetPalette) {
            gCurrentPalette = _gCurrentPalette;
            gTargetPalette = _gTargetPalette;
        }
        void set_gCurrentPalette(CRGBPalette16 _gCurrentPalette) {
            gCurrentPalette = _gCurrentPalette;
        }
        void set_gTargetPalette(CRGBPalette16 _gTargetPalette) {
            gTargetPalette = _gTargetPalette;
        }
        void setDivisions(uint8_t _divisions) 
        {
            divisions = _divisions;
            helper = new uint16_t[divisions];
            pos = new uint16_t[divisions];
            prevpos = new uint16_t[divisions];
            amountPerDivision = NUMLEDS/divisions;
            for(int i = 0; i < divisions; i++) {
                helper[i] = 0;
                pos[i] = 0;
                prevpos[i] = 0;
            }
            clear = false;
        }
        void updateEnd(uint16_t _end) {
            if(_end <= totalNUMLEDS && _end >= 0) {
                end = _end;
            }
            updateVars();
        }
        void updateStart(uint16_t _start) {
            if(_start < totalNUMLEDS && _start >= 0) {
                start = _start;
            }
            updateVars();
        }
        void updateSize(uint16_t _start, uint16_t _end) {
            if(_start < totalNUMLEDS) {
                start = _start;
            }
            if(_end < totalNUMLEDS) {
                end = _end;
            }
            updateVars();
        }
        void updateVars() {
            clearAll();
            calculateAmount();
            setDivisions(divisions);
            heat = new byte[NUMLEDS];
        }
        void clearAll() {
            *(helper) = 0;
            launching = true;
            sPseudotime = 0;
            sLastMillis = 0;
            sHue16 = 0;
            *(pos) = 0;
            *(prevpos) = 0;
            clear = true;
        }
        void calculateAmount() {
            if(start == NUM_LEDS) {
                start = 0;
            }
            if(end < start) {
                NUMLEDS = totalNUMLEDS + end - start ;
            } else {
                NUMLEDS = end - start;
            }
        }
        void setPaletteIndex(uint16_t _paletteIndex) {
            paletteIndex = _paletteIndex;
        }
        void setPatternIndex(uint16_t _patternIndex, bool clear) {
            patternIndex = _patternIndex;
            if(clear) {
                clearAll();
            }
        }
        int getPaletteIndex(){
            return paletteIndex;
        }
        int getPatternIndex() {
            return patternIndex;
        }

        uint16_t getPosition(uint16_t pos) {
            uint16 pos_;
            if(inverted){
                pos_ = end - 1 - pos;
            } else {
                pos_ = start+ pos;
            }
            pos_ = (pos_)%totalNUMLEDS;
            //Para duas fitas conectadas!
            if(hasTwoStrips && pos_ < 300) {
                pos_ = 299 - pos_;
            }
            return pos_;
        }

        uint16_t getPositionSameStrip(uint16_t pos) {
            uint16 pos_;
            if(inverted){
                pos_ = end - 1 - pos;
            } else {
                pos_ = start+ pos;
            }
            pos_ = (pos_)%totalNUMLEDS;
            return pos_;
        }

        String toString() {
            return 
                "index: " + String(index) + 
                ", start: " + String(start) +
                ", end: " + String(end) +
                ", NUMLEDS: " + String(NUMLEDS) +
                ", hasTwoStrips: " + String(hasTwoStrips);
        }
};