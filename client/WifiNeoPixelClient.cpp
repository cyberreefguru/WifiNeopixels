/*
 *  Client for controlling LEDs with Node MCU
 *
 */

#include "WifiNeoPixelClient.h"

// Internal Variables
static Configuration config;
static NeopixelWrapper controller;
static PubSubWrapper pubsubw;
static WifiWrapper wifiw;
static Menu menu;
static StatusIndicator statusIndicator;

// software timer instance
os_timer_t ledTimer;
volatile StatusEnum gStatus;
volatile uint8_t gLedCounter;
//volatile uint8_t gLedState;

// Indicates we have a command waiting for processing
static volatile uint8_t commandAvailable = false;

// Internal functions
void configure();
void parseCommand();
boolean initialize();
void ledTimerCallback(void *pArg);
void startupPause();


/**
 * Setup function
 *
 */
void setup()
{
	// Set global status as booting
	setStatus(Booting);

	// Configure serial port
	Serial.begin(115200);
	Serial.println(F("\n\rLED Controller Powered Up\n"));

	// Basic HW setup

	// This pauses the start process so the user can do things like
	// attach to the serial port and look at debug information :)
	pinMode(BUILT_IN_LED, OUTPUT);     // Initialize the  pin as an output

	// Flash internal LED to show we're alive
	startupPause();

	// Print status message
	Serial.println( F("Configuring status LEDs...") );
	if( statusIndicator.initialize() == 0 )
	{
		Serial.println( F("Error configuring status LEDs!") );
		while(1)
		{
			Helper::toggleLedTimed(50);
		}
	}

	// Set status LEDs to unknown - as we run code they will change to booting, then the actual status
	statusIndicator.setStatuses(Unknown); // we manually set this in cases timer does not initialize

	// Print status message
	Serial.println( F("Configuring timers and watchdog...") );

	// Setup and initiate internal timer
	os_timer_setfn(&ledTimer, ledTimerCallback, NULL);
	os_timer_arm(&ledTimer, 125, true);

	// Set watchdog time out to 8 seconds
	ESP.wdtDisable();
	ESP.wdtEnable(WDTO_8S);

	// Initialize the configuration object; configs stored in Flash
	Serial.println( F("Initializing configuration...") );
	statusIndicator.setStatus(Config, Booting);
    if( config.initialize() )
    {
        Serial.println( F("\nConfiguration initialized.") );
    	statusIndicator.setStatus(Config, Ok);
    }
    else
    {
    	Serial.println( F("\n**ERROR - failed to initialize configuration") );
    	setStatus(Error);
    	statusIndicator.setStatus(Config, Error);
    	Helper::error(); // never returns from here
    }

    // Initialize menu
	Serial.println( F("\nInitializing menu...") );
	menu.initialize(&config);

	// Initialize wifi, pubsub, and LEDs
	if( !initialize() )
	{
		menu.waitForConfig();
		configure();
	}

	// Set LED variables
	setStatus(Waiting);

	Serial.println(F("**Statistics**"));
	Serial.print(F("Free Heap - "));
	Serial.println(ESP.getFreeHeap() );
	Serial.print(F("Sketch Size - "));
	Serial.println(ESP.getSketchSize() );
	Serial.print(F("Free Space - "));
	Serial.println(ESP.getFreeSketchSpace() );

    Serial.println(F("** Initialization Complete **"));

}

/**
 * Loop function
 *
 */
void loop()
{
	uint8_t configFlag = 0;

	// calls yield and other must run code
	worker();

	// Check if command is available
	if( isCommandAvailable() )
	{
		parseCommand();
	}


	// Check if user wants to configure node
	if( Serial.available() )
	{
		configFlag = 1;
	}

	// Check if we need to enter command mode to reconfigure the node
	if( configFlag == 1)
	{
		configFlag = 0;
		setStatus(Configuring);
		configure();
	}

} // end loop

/**
 * Enters configure mode, then waits until power is cycled
 */
void configure()
{
	if( menu.configure() == 1 )
	{
		// NOTE: I could not get the device reboot reliably, so cycle the power
		//       is the best option.  Even "reset" and "restart" don't work reliably.
		Serial.println(F("\nUser Configuration Complete - please cycle power."));
		setStatus(Reset);
		Helper::error();
	}
	else
	{
		Serial.println( F("Configuration aborted!") );
	}

}


/**
 * Initializes the node.  Returns true if node configured properly, false otherwise.
 *
 */
boolean initialize()
{
	boolean configured = false;

	Serial.println(F("Configuring wifi..."));

	// Initialize WIFI
	statusIndicator.setStatus(Wifi, Booting);
	if( wifiw.initialize(&config) )
	{
		statusIndicator.setStatus(Wifi, Ok);

		Serial.println(F("Configuring queue..."));

		// Initialize pubsub
		statusIndicator.setStatus(Queue, Booting);
		if( pubsubw.initialize(&config, &wifiw) )
		{
			statusIndicator.setStatus(Queue, Ok);

			Serial.println(F("Configuring leds..."));

			// Initialize the LEDs
			statusIndicator.setStatus(Driver, Booting);
			if ( controller.initialize(config.getNumberLeds(), DEFAULT_INTENSITY) )
			{
				statusIndicator.setStatus(Driver, Ok);

				yield(); // give time to ESP
				Serial.print(F("\nLED Controller initialized..."));
				controller.fill(CRGB::White, true);
				Helper::delayWorker(250);
				controller.fill(CRGB::Black, true);
				Helper::delayWorker(250);
				controller.fill(CRGB::Red, true);
				Helper::delayWorker(250);
				controller.fill(CRGB::Black, true);
				Serial.println(F("LED Module Configured."));

				configured = true;
			}
			else
			{
				Serial.println(F("\nERROR - failed to configure LED module"));
				statusIndicator.setStatus(Driver, Error);
			}
		}
		else
		{
			Serial.println(F("\nERROR - failed to configure queue"));
			statusIndicator.setStatus(Queue, Error);
		}
	}
	else
	{
		Serial.println(F("\nERROR - failed to configure WIFI"));
		statusIndicator.setStatus(Wifi, Error);
	}

	return configured;

} // end initialize

/**
 * calls others functions while we are not busy.
 * This is required since we disabled interrupts
 * to use the FastLED library.
 *
 * TODO: re-enable interrupts and see what happens
 *
 */
void worker()
{
	pubsubw.work(); // process queue
	wifiw.work(); // check for OTA
	yield(); // give time to ESP
	ESP.wdtFeed(); // pump watch dog


	// check if we are connected to mqtt server
	if( pubsubw.connected() )
	{
		statusIndicator.setStatus(Queue, Ok);
	}
	else
	{
		statusIndicator.setStatus(Queue, Error);
		pubsubw.connect();
	}

	if( wifiw.connected() )
	{
		statusIndicator.setStatus(Wifi, Ok);
	}
	else
	{
		statusIndicator.setStatus(Wifi, Error);
	}

}


/**
 * Parses command buffer
 *
 */
void parseCommand()
{
	Command cmd;

	// Reset command available flag
	setCommandAvailable(false);
	setStatus(Processing);

#ifdef __DEBUG
	Serial.print( millis() );
	Serial.print(F(" - parsing command..."));
#endif

	if( cmd.parse( (uint8_t *)pubsubw.getBuffer() ) )
	{
		if( cmd.getNodeId() == 0 )
		{
			cmd.setNodeId( config.getNodeId() );
		}
#ifdef __DEBUG
		cmd.dump();
#endif
		switch(cmd.getCommand())
		{
		case CMD_SHOW:
			Serial.println(F("SHOW"));
			controller.show();
			break;
		case CMD_SET_PIXEL:
			Serial.println(F("SET_PIXEL"));
			controller.setPixel(cmd.getIndex(), cmd.getOnColor(), cmd.getShow() );
			break;
		case CMD_FILL:
			Serial.println(F("FILL"));
			controller.fill(cmd.getOnColor(), true);
			break;
		case CMD_FILL_PATTERN:
			Serial.println(F("FILL_PATTERN"));
			controller.fillPattern(cmd.getPattern(), cmd.getOnColor(), cmd.getOffColor());
			break;
		case CMD_PATTERN:
			Serial.println(F("PATTERN"));
			controller.rotatePattern(cmd.getRepeat(),cmd.getDuration(), cmd.getPattern(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime());
			break;
		case CMD_WIPE:
			Serial.println(F("WIPE"));
			controller.wipe(cmd.getRepeat(),cmd.getDuration(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime(), cmd.getClearAfter(), cmd.getClearEnd());
			break;
		case CMD_SCROLL:
			Serial.println(F("SCROLL"));
			controller.scrollPattern(cmd.getRepeat(),cmd.getDuration(), cmd.getPattern(), cmd.getPatternLength(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime(), cmd.getClearAfter(), cmd.getClearEnd());
			break;
		case CMD_BOUNCE:
			Serial.println(F("BOUNCE"));
			controller.bounce(cmd.getRepeat(), cmd.getDuration(), cmd.getPattern(), cmd.getPatternLength(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime(), cmd.getBounceTime(), cmd.getClearAfter(), cmd.getClearEnd());
			break;
		case CMD_MIDDLE:
			controller.middle(cmd.getRepeat(),cmd.getDuration(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime(), cmd.getClearAfter(), cmd.getClearEnd());
			Serial.println(F("MIDDLE"));
			break;
		case CMD_RANDOM_FLASH:
			Serial.println(F("RANDOM_FLASH"));
			controller.randomFlash(cmd.getRepeat(),cmd.getDuration(), cmd.getOnTime(), cmd.getOffTime(), cmd.getOnColor(), cmd.getOffColor(), cmd.getNumber() );
			break;
		case CMD_FADE:
			Serial.println(F("FADE"));
			controller.fade(cmd.getDirection(), cmd.getFadeIncrement(), cmd.getFadeTime(), cmd.getOnColor());
			break;
		case CMD_STROBE:
			Serial.println(F("STROBE"));
			controller.strobe(cmd.getRepeat(),cmd.getDuration(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime());
			break;
		case CMD_LIGHTNING:
			Serial.println(F("LIGHTNING"));
			controller.lightning(cmd.getRepeat(),cmd.getDuration(), cmd.getOnColor(), cmd.getOffColor());
			break;
		case CMD_STACK:
			Serial.println(F("STACK"));
			controller.stack(cmd.getRepeat(), cmd.getDuration(), cmd.getDirection(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getClearEnd() );
			break;
		case CMD_FILL_RANDOM:
			Serial.println(F("RANDOM FILL"));
			controller.fillRandom(cmd.getRepeat(), cmd.getDuration(), cmd.getOnColor(), cmd.getOffColor(), cmd.getOnTime(), cmd.getOffTime(), cmd.getClearAfter(), cmd.getClearEnd());
			break;
		case CMD_RAINBOW:
			Serial.println(F("RAINBOW"));
			controller.rainbow(cmd.getDuration(), cmd.getProbability(), cmd.getOnColor(), cmd.getOnTime(), cmd.getHueUpdateTime());
			break;
		case CMD_RAINBOW_FADE:
			Serial.println(F("RAINBOW_FADE"));
			controller.rainbowFade(cmd.getDuration(), cmd.getOnTime());
			break;
		case CMD_CONFETTI:
			Serial.println(F("CONFETTI"));
			controller.confetti(cmd.getDuration(), cmd.getOnColor(), cmd.getFadeBy(), cmd.getOnTime(), cmd.getHueUpdateTime() );
			break;
		case CMD_CYLON:
			Serial.println(F("CYLON"));
			controller.cylon(cmd.getRepeat(), cmd.getDuration(), cmd.getOnColor(), cmd.getFadeTime(), cmd.getFramesPerSecond(), cmd.getHueUpdateTime());
			break;
		case CMD_BPM:
			Serial.println(F("BPM"));
			controller.bpm(cmd.getDuration(), cmd.getOnTime(), cmd.getHueUpdateTime() );
			break;
		case CMD_JUGGLE:
			Serial.println(F("JUGGLE"));
			controller.juggle(cmd.getDuration(), cmd.getOnTime());
			break;
		case CMD_SET_INTENSITY:
			Serial.println(F("SET_INTENSITY"));
			controller.setIntensity( cmd.getIntensity() );
			break;
		case CMD_COMPLETE:
			Serial.println(F("COMPLETE"));
			break;
		case CMD_ERROR:
		default:
			Serial.println(F("ERROR - UNKNOWN COMMAND"));
			break;
		} // end switch

		// Send response with the command is complete
		if( cmd.getNotifyOnComplete() )
		{
			if( cmd.buildResponse( pubsubw.getBuffer() ) )
			{
				Serial.print(F("Publishing Completion Response: "));
				Serial.println((char *)config.getMyResponseChannel() );

				// Publish message to response channel
				pubsubw.publish( (char *)config.getMyResponseChannel() );
			}

		} // end if notify on complete

		// NOTE: Don't be foolish and send a command to "all" with relay set; strange things will happen
		if( cmd.getRelay() == true )
		{
			Serial.print( millis() );
			Serial.println(F(" - Relay Command"));
			if(  cmd.getRelayNodeSize() > 0 )
			{
				uint8_t *relayNodes = cmd.getRelayNodes(); // get relay node array
				uint8_t destNode = relayNodes[0]; // save the node we are relaying to

				if( destNode > 0 )
				{
					cmd.setNodeId( destNode ); // change node ID for command

					Serial.print( millis() );
					Serial.print(F(" - Relaying to: "));
					Serial.println( destNode );

					// Shift relay nodes left 1, removing destination from list
					// (we don't want to relay back to ourselves)
					cmd.shiftRelayNodes();

					// Build "new" command to relay to next node
					if( cmd.buildCommand( pubsubw.getBuffer() ) )
					{
						// Build channel to send it to
						char channel[STRING_SIZE];
						memset(channel, 0, STRING_SIZE);
						sprintf((char *)channel, DEFAULT_CHANNEL_MY, destNode );

						// Send command
						Serial.print( millis() );
						Serial.print(F(" - Publishing Relay Command: "));
						Serial.println(channel );

						pubsubw.publish( channel );
					}
				}
				else
				{
					Serial.println(F("ERROR - Destination Node = 0"));
				}
			}
			else
			{
				Serial.println(F("** END OF RELAY CHAIN **"));
			}

		} // end if relay

	} // end if cmd parse = true

	Serial.print( millis() );
	Serial.println(F(" - Command Complete"));

	setStatus(Waiting);
}

/**
 * Returns flag; true=new command is available
 */
boolean isCommandAvailable()
{
	return commandAvailable;
}

/**
 * Sets command available flag
 *
 */
void setCommandAvailable(boolean flag)
{
	commandAvailable = flag;
}


/**
 * Delay function with command check
 */
uint8_t commandDelay(uint32_t time)
{
	boolean cmd = isCommandAvailable();
	if( time == 0 ) return cmd;

	if( !cmd )
	{
		for (uint32_t i = 0; i < time; i++)
		{
			delay(1);
			worker();
			cmd = isCommandAvailable();
			if (cmd)
			{
				break;
			}
		}
	}
	return cmd;

}

/**
 * Flashes LED during startup to tell user we're alive
 */
void startupPause()
{
	uint8_t i;

	Helper::setLed(OFF);
	for(i=0; i< 10; i++)
	{
		Helper::toggleLed();
		delay(200);
	}
	Helper::setLed(OFF);

}

/**
 * C function that calls class-level callback
 * Admittedly a hack, but it works
 *
 */
void pubsubCallback(char* topic, byte* payload, unsigned int length)
{
//	Serial.println("Calling pubsub callback...");
	pubsubw.callback(topic, payload, length);

}

/**
 * Software timer callback - controls the general status LED
 *
 * Note: overly complicated but pleasing to the eyes ;)
 */
void ledTimerCallback(void *pArg)
{
	gLedCounter += 1;

	if( gStatus == Error || gStatus == Reset )
	{
		// Flash every other tick
		switch( gLedCounter )
		{
		case 1:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		case 3:
			statusIndicator.setGeneralStatus(None);
			break;
		case 5:
			gLedCounter = 0;
			break;
		default:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		}
	}
	else if( gStatus == Booting )
	{
		statusIndicator.setGeneralStatus(gStatus);
		gLedCounter = 0;
	}
	else if( gStatus == Uploading )
	{
		// Flash every tick
		switch( gLedCounter )
		{
		case 1:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		case 2:
			statusIndicator.setGeneralStatus(None);
			gLedCounter = 0;
			break;
		default:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		}
	}
	else
	{
		// Create heart beat - on 1 cycle, off 2 cycles, on 1 cycle, off 6 cycles
		switch( gLedCounter )
		{
		case 1:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		case 2:
			statusIndicator.setGeneralStatus(None);
			break;
		case 4:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		case 5:
			statusIndicator.setGeneralStatus(None);
			break;
		case 11:
			gLedCounter = 0;
			break;
		default:
			statusIndicator.setGeneralStatus(gStatus);
			break;
		}
	}

} // End of timerCallback

void setStatus(volatile StatusEnum status)
{
	gStatus = status;
	gLedCounter = 0;
}

volatile StatusEnum getStatus()
{
	return gStatus;
}
