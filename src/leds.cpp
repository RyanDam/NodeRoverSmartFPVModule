#include "main.h"

void setLED(byte brightness) {
	ledcWrite(LED_CHANNEL, brightness);   // change LED brightness (0 - 255)
};

void setupLEDPWM() {
    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RES);
    ledcAttachPin(LED_BUILTIN, LED_CHANNEL);
    setLED(LED_INIT_BRIGHTNESS);
};