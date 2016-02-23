/*
 * Menu.h
 *
 *  Created on: Feb 21, 2016
 *      Author: tsasala
 */

#ifndef MENU_H_
#define MENU_H_

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "Command.h"
#include "Configuration.h"

#include "PubSubWrapper.h"
#include "NeoPixelWrapper.h"
#include "WifiWrapper.h"
#include "Helper.h"



class Menu
{
public:
	Menu();
	uint8_t initialize(Configuration* config, WifiWrapper* wifi, PubSubWrapper* pubsub, NeopixelWrapper* controller);
	void configure();
	void waitForWifiConfig();
	void waitForPubSubConfig();

protected:
	Configuration* config;
	WifiWrapper* wifi;
	PubSubWrapper* pubsub;
	NeopixelWrapper* controller;

};

#endif /* MENU_H_ */
