/*
 * Project: CS:GO C4 Prop Replica
 * Author: Richard Moore - https://blog.radboogie.com - richard@radboogie.com
 * Date: December 2022
 * Blog: https://blog.radboogie.com/make-your-own-csgo-c4-prop-part-4-software/
 * YouTube: https://www.youtube.com/@radboogie
 * 
 * Description:
 * This Arduino sketch controls a Counter-Strike: Global Offensive (CS:GO) C4 prop replica.
 * The device simulates a bomb with a keypad for entering a password, an LCD display for
 * feedback, a countdown timer, sound effects, and LED indicators. It features multiple
 * states including initialization, password entry, countdown, explosion, and clock mode.
 * The prop uses a real-time clock (RTC) for timekeeping, supports day/night LCD brightness
 * settings stored in EEPROM, and includes a configuration mode via serial communication.
 * The project is designed for hobbyists and cosplayers to create an interactive CS:GO-themed
 * prop.
 *
 * Dependencies:
 * - LiquidCrystal (for 16x2 LCD display)
 * - WTV020SD16P (for WTV020-SD-16P audio module control)
 * - Keypad (for 4x3 keypad input)
 * - DS3231 (for DS3231 RTC module)
 * - RTClib (for RTC timekeeping functions)
 * - Wire (for I2C communication, included in Arduino core)
 * - PCF8574 (for PCF8574 I2C I/O expander)
 * - EEPROM (for persistent storage, included in Arduino core)
 *
 * Hardware Requirements:
 * - Arduino board (e.g., Uno or compatible)
 * - 16x2 LCD display
 * - WTV020-SD-16P audio module with SD card
 * - 4x3 keypad
 * - DS3231 RTC module
 * - PCF8574 I2C I/O expander
 * - Red and green LEDs (controlled via PCF8574)
 * - Backlight-enabled LCD with PWM control
 * - Appropriate wiring and power supply
 *
 * Basic Functionality:
 * - Initializes with a startup sequence displaying "Bang-OMatic 5000" on the LCD.
 * - Accepts a 7-digit password via keypad (default correct password: "7355608").
 * - Enters countdown mode (40 seconds) upon correct password entry, with beeping and
 *   flashing red LED.
 * - Simulates explosion with sound effects and green LED if countdown reaches zero.
 * - Allows defusal during countdown, playing defusal sounds and displaying success.
 * - Times out to clock mode after 30 seconds of inactivity, showing current time.
 * - Supports day/night LCD brightness modes based on RTC time and EEPROM settings.
 * - Enters configuration mode via long press of '*' key for serial-based settings
 *   (e.g., brightness, time, day/night schedules).
 *
 * License:
 * MIT License
 *
 * Copyright (c) 2025 Richard Moore - https://github.com/RadBoogie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// include the library code:
#include <LiquidCrystal.h>
#include <WTV020SD16P.h>
#include <Keypad.h>
#include <DS3231.h>
#include <Wire.h>
#include <PCF8574.h>
#include <EEPROM.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Constants to define the used pins.
static const uint8_t resetPin = 10; // The pin number of the reset pin.
static const uint8_t clockPin = 11; // The pin number of the clock pin.
static const uint8_t dataPin = 12; // The pin number of the data pin.
static const uint8_t busyPin = 13; // The pin number of the busy pin.

static const int EEPROM_DAY_BRIGHT_ADDRESS = 0;
static const int EEPROM_NIGHT_BRIGHT_ADDRESS = 1;
static const int EEPROM_DAY_START_HOUR_ADDRESS = 2;
static const int EEPROM_DAY_START_MINUTE_ADDRESS = 3;
static const int EEPROM_NIGHT_START_HOUR_ADDRESS = 4;
static const int EEPROM_NIGHT_START_MINUTE_ADDRESS = 5;

// Instance of WTV020SD16P
WTV020SD16P wtv020sd16p(resetPin, clockPin, dataPin, busyPin);

RTClib RealTimeClock;

DS3231 Rtc;

PCF8574 I2CIoExtender(0x20); // I2C controlled digital IO extender for front LED

int LCD_BACKLIGHT_PIN = 3;

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = { A1, 0, 2, A3 }; //connect to the row pinouts of the keypad
byte colPins[COLS] = { A2, A0, 1 }; //connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

unsigned long lastMilliCount;

unsigned long splatKeyPressedMillisCount;

bool splatKeyPressed = false;

bool serialMode = false;

String passwordString = "*******";

enum States {
	Initialising,
	EnterPassword,
	Countdown,
	Exploding,
	Configuring,
	Clock
};

States state = Initialising;

const int BEEP_SOUND = 0;
const int TERRORISTS_WIN_SOUND = 1;
const int KEY_PRESS_SOUND = 2;
const int ABOUT_TO_BLOW_SOUND = 3;
const int TIMES_UP_SOUND = 4;
const int EXPLODE_SOUND = 5;
const int C4_INITIATE_SOUND = 6;
const int DEFUSING_USA_SOUND = 7;
const int DEFUSED_SOUND = 8;
const int DEFUSED_USA_SOUND = 9;
const int COUNTER_TERRORISTS_WIN_SOUND = 10;


void setup() {
	// Start the I2C interface
	Wire.begin();

	// set up the LCD's number of columns and rows:
	lcd.begin(16, 2);

	pinMode(LCD_BACKLIGHT_PIN, OUTPUT);

	SetLCDBrightness();

	wtv020sd16p.reset();
	delay(600);

	keypad.setHoldTime(3000);

	ResetClockTimeout();
}

void SetLCDBrightness()
{
	// Determine if day or night...

	bool isAfterStartTime = false;
	bool isBeforeEndTime = false;
	bool isTwelveHour;
	bool isPm;

	int currentHour = Rtc.getHour(isTwelveHour, isPm);
	int currentMinute = Rtc.getMinute();

	int dayStartHour = EEPROM.read(EEPROM_DAY_START_HOUR_ADDRESS);
	int dayStartMinute = EEPROM.read(EEPROM_DAY_START_MINUTE_ADDRESS);
	int nightStartHour = EEPROM.read(EEPROM_NIGHT_START_HOUR_ADDRESS);
	int nightStartMinute = EEPROM.read(EEPROM_NIGHT_START_MINUTE_ADDRESS);

	String currentHourString = String(currentHour);
	String currentMinuteString = currentMinute < 10 ? "0" + String(currentMinute) : String(currentMinute);
	int convertedCurrentTime = (currentHourString + currentMinuteString).toInt();

	String startHourString = String(dayStartHour);
	String startMinuteString = dayStartMinute < 10 ? "0" + String(dayStartMinute) : String(dayStartMinute);
	int convertedStartTime = (startHourString + startMinuteString).toInt();

	if (convertedCurrentTime >= convertedStartTime)
	{
		isAfterStartTime = true;
	}

	String endHourString = String(nightStartHour);
	String endMinuteString = nightStartMinute < 10 ? "0" + String(nightStartMinute) : String(nightStartMinute);
	int convertedEndTime = (endHourString + endMinuteString).toInt();

	if (convertedCurrentTime < convertedEndTime)
	{
		isBeforeEndTime = true;
	}

	if (isAfterStartTime && isBeforeEndTime)
	{ 
		analogWrite(LCD_BACKLIGHT_PIN, EEPROM.read(EEPROM_DAY_BRIGHT_ADDRESS));
	}
	else
	{
		analogWrite(LCD_BACKLIGHT_PIN, EEPROM.read(EEPROM_NIGHT_BRIGHT_ADDRESS));
	}
}

void SetLCDBrightness(int brightness)
{
	analogWrite(LCD_BACKLIGHT_PIN, brightness);
}

unsigned long clockTimeoutTimeElapsed;
int clockTimeoutMillis = 30000; 

void ResetClockTimeout()
{
	clockTimeoutTimeElapsed = millis();
}

void DoInitialisationSequence()
{
	// Print a message to the LCD.
	LcdPrintLeftJustified("Bang-OMatic 5000");

	delay(2000);

	LcdPrintLeftJustified("Have nice day!");

	delay(2000);

	PlaySoundAsync(C4_INITIATE_SOUND);

	RedLed(true);
	delay(100);
	RedLed(false);
	delay(100);
	GreenLed(true);
	delay(100);
	GreenLed(false);

	LcdPrintLeftJustified("         *******");

	passwordString = "*******";

	state = EnterPassword;

	ResetCountdownRegisters();
}

void loop() {
	// Once we're in config mode the keypad is ignored. To exit config mode 
	// the user restarts the device.
	if (state == Configuring)
	{
		return;
	}

	if (state == Initialising)
	{
		DoInitialisationSequence();
	}

	if (keypad.getKeys())
	{
		for (int i = 0; i < LIST_MAX; i++)   // Scan the whole key list.
		{
			if (keypad.key[i].stateChanged)   // Only find keys that have changed state.
			{
				switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
				case PRESSED:
					PlaySoundAsync(KEY_PRESS_SOUND);

					ResetClockTimeout();

					if (state == Countdown)
					{
						DefuseBomb();
						return;
					}

					if (state == Clock)
					{
						state = Initialising;
						return;
					}

					AddCharacterToPassword(keypad.key[i].kchar);
					break;
				case HOLD:
					break;
				case RELEASED:
					break;
				case IDLE:
					break;
				}

				if (keypad.key[i].kstate == HOLD && keypad.key[i].kchar == '*')
				{
					Serial.begin(9600);
					state = Configuring;

					LcdPrintLeftJustified("USB MODE ENABLED");
				}
			}
		}
	}

	if (state == Countdown)
	{
		ResetClockTimeout();
		DoCountdown();
	}

	if (state == Exploding)
	{
		ResetClockTimeout();
		DoExploding();
	}

	// After 30 seconds we timeout and become a clock...
	if (millis() > clockTimeoutTimeElapsed + clockTimeoutMillis)
	{
		state = Clock;
	}

	if (state == Clock)
	{
		DoClock();
	}

	lastMilliCount = millis();

}

void DefuseBomb()
{
	delay(500);

	LcdPrintLeftJustified("Defusing.");

	delay(500);

	PlaySoundAsync(DEFUSING_USA_SOUND);

	LcdPrintLeftJustified("Defusing..");
	delay(1000);
	LcdPrintLeftJustified("Defusing...");
	delay(1000);
	LcdPrintLeftJustified("Defusing....");
	delay(1000);
	LcdPrintLeftJustified("Defusing.....");
	delay(1000);

	PlaySoundAsync(DEFUSED_SOUND);

	delay(1500);


	LcdPrintLeftJustified("BombOS v2.1");
	delay(1500);
	LcdPrintLeftJustified(" -=SAFE MODE=-");

	PlaySoundAsync(DEFUSED_USA_SOUND);

	delay(2800);

	PlaySoundAsync(COUNTER_TERRORISTS_WIN_SOUND);

	LcdPrintLeftJustified("CT Win!");

	delay(5000);

	state = Clock;
}

unsigned long clockDisplayTimeElapsed;

void DoClock()
{
	String time = "";

	// Write time to display...
	if (millis() > clockDisplayTimeElapsed + 1000)
	{
		clockDisplayTimeElapsed = millis();

		DateTime now = RealTimeClock.now();

		int hour = now.hour();

		if (hour < 10)
		{
			time += "0";
		}

		time += hour;
		time += ":";

		int minute = now.minute();

		if (minute < 10)
		{
			time += "0";
		}

		time += minute;
		time += ":";


		int second = now.second();

		if (second < 10)
		{
			time += "0";
		}

		time += second;

		LcdPrintLeftJustified("    " + time);

		// Handle day or night mode LCD brightness...
		SetLCDBrightness();
	}
}

void PlaySoundAsync(int fileNumber)
{
	wtv020sd16p.asyncPlayVoice(fileNumber);
}

unsigned long countdownTimeElapsed;
int countDownHundredths = 100;
int countDownSeconds = 40;
void DoCountdown()
{
	if (millis() > countdownTimeElapsed + 50)
	{
		countdownTimeElapsed = millis();

		// Update display...
		String time = "00:";

		if (countDownSeconds < 10)
		{
			time += "0";
		}

		time += countDownSeconds;
		time += ":";

		// Just print random numbers for the hundredths, they're too quick to read anyway...
		time += random(9);
		time += random(9);

		LcdPrintTime(time);

		// Count down hundredths and seconds (steps of 5 hundredths)
		countDownHundredths -= 5;

		if (countDownSeconds <= 0)
		{
			//BOOM!
			state = Exploding;

			LcdPrintTime("00:00:00");

			return;
		}

		if (countDownHundredths <= 0)
		{


			countDownSeconds--;

			countDownHundredths = 100;
		}
	}

	FlashRedLedAndBeep(countDownSeconds);
}

void DoExploding()
{
	GreenLed(true);

	PlaySoundAsync(ABOUT_TO_BLOW_SOUND);

	delay(1500);

	PlaySoundAsync(TIMES_UP_SOUND);

	delay(1200);

	LcdPrintLeftJustified(" WARRANTY VOID!");

	PlaySoundAsync(EXPLODE_SOUND);

	delay(4000);

	LcdPrintLeftJustified("Terrorists win!");

	PlaySoundAsync(TERRORISTS_WIN_SOUND);

	ResetCountdownRegisters(); // for next time

	delay(5000);

	GreenLed(false);
	state = Clock;
}

unsigned long flashRedLedAndBeepTimeElapsed;
int flashRedLedAndBeepIntervalMillis = 1000;

unsigned long flashRedLedTimeElapsed;
int flashRedLedIntervalMillis = 50;
bool redLedIsOn = false;

void FlashRedLedAndBeep(int currentTimeSeconds)
{
	// See if it's time for a beep?
	if (millis() > flashRedLedAndBeepTimeElapsed + flashRedLedAndBeepIntervalMillis)
	{
		// Limit the final speed so it doesn't beep too fast!
		if (flashRedLedAndBeepIntervalMillis > 185)
		{
			flashRedLedAndBeepIntervalMillis -= 12;
		}

		flashRedLedAndBeepTimeElapsed = millis();

		redLedIsOn = true;
		RedLed(true);
		flashRedLedTimeElapsed = millis();

		PlaySoundAsync(BEEP_SOUND);
	}

	// Handle turning red led off after flash elapsed
	if (redLedIsOn)
	{
		if (millis() > flashRedLedTimeElapsed + flashRedLedIntervalMillis)
		{
			RedLed(false);
			redLedIsOn = false;
		}
	}
}

void ResetCountdownRegisters()
{
	countDownHundredths = 100;
	countDownSeconds = 40;

	flashRedLedAndBeepIntervalMillis = 1000;
}

void RedLed(bool on)
{
	if (on)
	{
		I2CIoExtender.write(0, LOW);
		I2CIoExtender.write(1, HIGH);
	}
	else
	{
		I2CIoExtender.write(0, HIGH);
		I2CIoExtender.write(1, HIGH);
	}
}

void GreenLed(bool on)
{
	if (on)
	{
		I2CIoExtender.write(0, HIGH);
		I2CIoExtender.write(1, LOW);
	}
	else
	{
		I2CIoExtender.write(0, HIGH);
		I2CIoExtender.write(1, HIGH);
	}
}

String previousString = "";
void LcdPrintLeftJustified(String string)
{
	if (string == previousString)
	{
		return;
	}

	lcd.clear();

	int row = 0;
	int colModifier = 0;
	for (int i = 0; i < string.length(); i++)
	{
		lcd.setCursor(i - colModifier, row);
		lcd.print(string[i]);

		if (i >= 7)
		{
			row++;
			colModifier = 8;
		}
	}

	previousString = string;
}

void AddCharacterToPassword(char character)
{
	passwordString = passwordString.substring(1); // Removes leftmost char
	passwordString += character;

	PrintPassword();

	if (passwordString == "7355608")
	//if (passwordString == "******1")
	{
		// Start countdown!
		state = Countdown;
	}
}

void PrintPassword()
{
	LcdPrintLeftJustified("         " + passwordString);
}

String previousTimeString = "";
void LcdPrintTime(String string)
{
	if (string == previousTimeString)
	{
		return;
	}

	for (int i = 0; i < string.length(); i++)
	{
		lcd.setCursor(i, 1);
		lcd.print(string[i]);
	}

	previousTimeString = string;
}

void serialEvent()
{
	while (Serial.available())
	{
		ParseSerialMessage(Serial.readString());
	}
}

void(*resetFunc) (void) = 0;

void ParseSerialMessage(String message)
{
	// First get command
	int delimiterIndex = message.indexOf(':'); // 3

	String command = message.substring(0, delimiterIndex);

	int commandLength = command.length(); // 3 for LCD
	int messageLength = message.length(); // 8 for LCD:123

	String value = message.substring(delimiterIndex + 1, messageLength - 1);

	if (command == "LCD")
	{
		SetLCDBrightness(value.toInt());
	}

	if (command == "GET_LCD_DAY")
	{
		Serial.println(EEPROM.read(EEPROM_DAY_BRIGHT_ADDRESS));
	}

	if (command == "GET_LCD_NIGHT")
	{
		Serial.println(EEPROM.read(EEPROM_NIGHT_BRIGHT_ADDRESS));
	}

	if (command == "SET_LCD_DAY")
	{
		EEPROM.write(EEPROM_DAY_BRIGHT_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}

	if (command == "SET_LCD_NIGHT")
	{
		EEPROM.write(EEPROM_NIGHT_BRIGHT_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}

	if (command == "SET_DAY_START_HOUR")
	{
		EEPROM.write(EEPROM_DAY_START_HOUR_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}

	if (command == "SET_DAY_START_MINUTE")
	{
		EEPROM.write(EEPROM_DAY_START_MINUTE_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}

	if (command == "SET_NIGHT_START_HOUR")
	{
		EEPROM.write(EEPROM_NIGHT_START_HOUR_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}

	if (command == "SET_NIGHT_START_MINUTE")
	{
		EEPROM.write(EEPROM_NIGHT_START_MINUTE_ADDRESS, value.toInt());
		Serial.println("ACK:");
	}


	if (command == "SET_HOUR")
	{
		Rtc.setHour(value.toInt());
		Serial.println("ACK:");
	}

	if (command == "GET_HOUR")
	{
		bool isTwelveHour;
		bool isPm;

		Serial.println(Rtc.getHour(isTwelveHour, isPm));
	}

	if (command == "SET_MINUTE")
	{
		Rtc.setMinute(value.toInt());
		Serial.println("ACK:");
	}

	if (command == "GET_MINUTE")
	{
		Serial.println(Rtc.getMinute());
	}

	if (command == "GET_DAY_START_HOUR")
	{
		Serial.println(EEPROM.read(EEPROM_DAY_START_HOUR_ADDRESS));
	}

	if (command == "GET_DAY_START_MINUTE")
	{
		Serial.println(EEPROM.read(EEPROM_DAY_START_MINUTE_ADDRESS));
	}

	if (command == "GET_NIGHT_START_HOUR")
	{
		Serial.println(EEPROM.read(EEPROM_NIGHT_START_HOUR_ADDRESS));
	}

	if (command == "GET_NIGHT_START_MINUTE")
	{
		Serial.println(EEPROM.read(EEPROM_NIGHT_START_MINUTE_ADDRESS));
	}


	if (command == "REBOOT")
	{
		resetFunc();
	}
}

