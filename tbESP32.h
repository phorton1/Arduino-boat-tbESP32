//----------------------------------------------
// constants for tbESP32
//----------------------------------------------
// started as teenyBoat Serial-Telnet forwarding IOT device.
// SDCard uses ESP32 standard SPI mosi(23) miso(19) sclk(18)
// Uses alternat pins for HardwareSerial 2

#pragma once

#include <myIOTDevice.h>


//=========================================================
// pins
//=========================================================

#define PIN_SERIAL2_RX		25
#define PIN_SERIAL2_TX		26
#define PIN_ENABLE_UDP		13


//------------------------
// myIOT definition
//------------------------

#define ESP32_TELNET			"esp32_telnet"
#define ESP32_TELNET_VERSION	"et1.0"
#define ESP32_TELNET_URL		"https://github.com/phorton1/Arduino-boat-tbESP32"

#define ID_ENABLE_UDP				"ENABLE_UDP"
#define ID_STATUS					"STATUS"


class tbESP32 : public myIOTDevice
{
public:

    tbESP32() {}
    ~tbESP32() {}

    //	virtual void setup() override;
    //	virtual void loop() override;
	//	virtual bool showDebug(String path) override;

	static String	_status_str;
	static bool		_enable_udp;

	static void onUDPEnableChanged(const myIOTValue *value, bool val);
};


extern tbESP32 *tb_esp32;



