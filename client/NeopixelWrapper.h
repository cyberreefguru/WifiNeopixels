/*
 * NeoPixelWrapper.h
 *
 *  Created on: Sep 12, 2015
 *      Author: tsasala
 */

#ifndef NEOPIXELWRAPPER_H_
#define NEOPIXELWRAPPER_H_

#include <Arduino.h>
#include <FastLed.h>

// What HW platform are we dealing with?
#ifdef __ESP8266
#define DEFAULT_LED_PIN 	D8
#endif
#ifdef __ARDUINO
#define DEFAULT_LED_PIN		3
#endif

// What LED string are we dealing with
#ifdef __LED_STRING
#define DEFAULT_CONTROLLER	WS2812
#endif

#ifdef __LED_STRIP
#define DEFAULT_CONTROLLER	NEOPIXEL
#endif



#define WHITE	CRGB::White
#define BLACK	CRGB::Black
#define RED		CRGB::Red
#define BLUE	CRGB::Blue
#define GREEN	CRGB::Green
#define MAGENTA	CRGB::Magenta
#define CYAN	CRGB::Cyan
#define YELLOW	CRGB::Yellow
#define ORANGE	CRGB::Orange
#define PURPLE	CRGB::Purple
#define RAINBOW	CRGB::Black

#define DOWN 	0
#define UP		1
#define LEFT	0
#define RIGHT	1
#define IN		0
#define OUT		1

#define DEFAULT_FPS 		120
#define DEFAULT_INTENSITY	200

class NeopixelWrapper
{
public:
	NeopixelWrapper();
	boolean initialize(uint8_t numLeds, uint8_t intensity);
	boolean reinitialize(uint8_t numLeds, uint8_t intensity);

	void setFramesPerSecond(uint8_t fps);
	uint8_t getFramesPerSecond();
	void setHueUpdateTime(uint8_t updateTime);
	uint8_t getHueUpdateTime();
	void setIntensity(uint8_t i);
	uint8_t getIntensity();

	void show();

	CRGB getPixel(int16_t index);
	void setPixel(int16_t index, CRGB color, uint8_t show);
    void fill(CRGB color, uint8_t show);
    void fillPattern(uint8_t pattern, CRGB onColor, CRGB offColor);

    void rotatePattern(uint16_t repeat, uint8_t pattern, uint8_t direction, CRGB onColor, CRGB offColor, uint32_t onTime, uint32_t offTime);
    void scrollPattern(uint8_t pattern, uint8_t direction, CRGB onColor, CRGB offColor, uint32_t onTime, uint32_t offTime, uint8_t clearAfter, uint8_t clearEnd);
    void bounce(uint16_t repeat, uint8_t pattern, uint8_t direction, CRGB onColor, CRGB offColor, uint32_t onTime, uint32_t offTime, uint32_t bounceTime, uint8_t clearAfter, uint8_t clearEnd);
    void middle(uint16_t repeat, uint8_t direction, CRGB color1, CRGB color2, uint32_t onTime, uint32_t offTime, uint8_t clearAfter, uint8_t clearEnd);
    void randomFlash(uint32_t runTime, uint32_t onTime, uint32_t offTime, CRGB onColor, CRGB offColor);
    void fade(uint8_t direction, uint8_t fadeIncrement, uint32_t time, CRGB color);
    void strobe(uint32_t duration, CRGB onColor, CRGB offColor, uint32_t onTime, uint32_t offTime );
    void lightning(CRGB onColor, CRGB offColor);

    void rainbow(uint32_t runTime,uint8_t glitterProbability, CRGB glitterColor, uint8_t fps);
    void rainbowFade(uint32_t runTime, uint8_t fps);
	void confetti(uint32_t runTime,CRGB color, uint8_t fadeBy, uint8_t fps);
	void cylon(uint16_t repeat, CRGB color, uint32_t fadeTime, uint8_t fps);
	void bpm(uint32_t runTime, uint8_t fps);
	void juggle(uint32_t runTime, uint8_t fps);

protected:
	CRGB *leds;
	uint8_t gHue; // rotating "base color" used by many of the patterns
//	uint8_t sparkleCount;
//	uint8_t frameWaitTime;
	uint8_t gHueUpdateTime;
//	uint8_t maxIntensity;
//	uint8_t maxFps;

	void setPixel(int16_t index, CRGB color);
	void setPatternTimed(int16_t index, uint8_t pattern, CRGB onColor, CRGB offColor, uint32_t onTime, uint32_t offTime, uint8_t clearAfter);
	void setPixelTimed(int16_t index, CRGB newColor, uint32_t onTime, uint32_t offTime, uint8_t clearAfter);
	void setPattern(int16_t startIndex, uint8_t length, uint8_t pattern, uint8_t patternLength, CRGB onColor, CRGB offColor, uint8_t show);


};

//end of add your includes here
#ifdef __cplusplus
extern "C"
{
#endif

extern uint8_t isCommandAvailable();
extern uint8_t commandDelay(uint32_t time);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NEOPIXELWRAPPER_H_ */
