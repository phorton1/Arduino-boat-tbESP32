//-----------------------------------------------------------------------
// tbESP32.ino
//-----------------------------------------------------------------------
// Bi-directionaly UDP<->Serial myIOT device to connect teensyBoat.ino
// to teensyBoat.pm over UDP. The main reason for using UDP is rapid
// updates of text/binary information from the ino program.
//
// There are definite negatives to using UDP including, not least of all,
// that it is not guaranteed, nor guaranteed in order.  Also, leaving a
// port open on the LAN is iffy at best.
//
// Coming from the laptop, commands received by UDP are generally OK,
// They either work, or they don't.
//
// The program listens to the teensy serial port and builds a UDP packet
// that contains 'full' decoded packets (binary or text lines) from the
// teensy.   It sends those out based on a frequency or a size limit.
//

#include "tbESP32.h"
#include <myIOTLog.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define DEBUG_TB_SERIAL			       0		// 1=lines, 2=every byte
#define DEBUG_UDP_TO_TEENSY            0
#define DEBUG_TEENSY_TO_UDP            0

#define UDP_SEND_INTERVAL              200     // ms
#define UDP_SEND_MAX                   1024    // bytes

HardwareSerial tbSerial(2);


//------------------------------
// myIOT setup
//------------------------------

static valueIdType dash_items[] = {
    ID_ENABLE_UDP,
	ID_STATUS,
    0
};

static valueIdType config_items[] = {
    0
};

const valDescriptor tbESP32_values[] =
{
    {ID_DEVICE_NAME,    VALUE_TYPE_STRING,  VALUE_STORE_PREF,      VALUE_STYLE_REQUIRED,   NULL,    NULL,   ESP32_TELNET },
        // override base class element
	{ID_ENABLE_UDP,     VALUE_TYPE_BOOL,  	VALUE_STORE_PREF,      VALUE_STYLE_NONE,   	   (void *) &tbESP32::_enable_udp,    (void *) tbESP32::onUDPEnableChanged,   },
    {ID_STATUS,         VALUE_TYPE_STRING,  VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &tbESP32::_status_str,    NULL,   },
};



#define NUM_TBESP32_VALUES (sizeof(tbESP32_values)/sizeof(valDescriptor))

tbESP32 *tb_esp32;
String tbESP32::_status_str;
bool   tbESP32::_enable_udp;


void tbESP32::onUDPEnableChanged(const myIOTValue *value, bool val)
{
	LOGI("onUDPEnableChanged(%d)",val);
	digitalWrite(PIN_ENABLE_UDP,val);
}


//--------------------------------
// main
//--------------------------------

void setup()
{
	pinMode(PIN_ENABLE_UDP,OUTPUT);
	digitalWrite(PIN_ENABLE_UDP,0);

    Serial.begin(MY_IOT_ESP32_CORE == 3 ? 115200 : 921600);
    delay(1000);

    LOGU("tbESP32.ino setup() started on core(%d)",xPortGetCoreID());
    tb_esp32 = new tbESP32();
    tb_esp32->addValues(tbESP32_values,NUM_TBESP32_VALUES);
    tb_esp32->setTabLayouts(dash_items,config_items);

    LOGU("calling tb_esp32->setup()",0);
    tb_esp32->setup();

    LOGU("opening Serial2",0);
    tbSerial.begin(921600, SERIAL_8N1, PIN_SERIAL2_RX, PIN_SERIAL2_TX);

	if (tb_esp32->getBool(ID_ENABLE_UDP))
	{
		LOGI("setup setting PIN_ENABLE_UDP HIGH",0);
		digitalWrite(PIN_ENABLE_UDP,1);
	}

	LOGU("tbESP32.ino setup() finished",0);

}


//--------------------------------
// UDP
//--------------------------------

WiFiUDP udp;
const int UDP_PORT = 5005;
static bool udp_started = 0;

// State for packet assembly
static bool inBinary = false;
static uint16_t binLength = 0;
static uint16_t binReceived = 0;
static uint8_t binBuffer[UDP_SEND_MAX];   // temporary buffer for one binary message
static String textBuffer;

// UDP transmit buffer
static uint8_t udpBuffer[UDP_SEND_MAX];
static size_t udpBufferLen = 0;
static unsigned long lastSendMillis = 0;


static void flushUDP()
{
    if (udpBufferLen > 0)
    {
        udp.beginPacket("10.237.50.255", UDP_PORT);
        udp.write(udpBuffer, udpBufferLen);
        udp.endPacket();

        if (DEBUG_TEENSY_TO_UDP)
            LOGI("ESP32 flushed UDP packet(%d bytes)", udpBufferLen);

        udpBufferLen = 0;
        lastSendMillis = millis();
    }
}


static void addMessageToUDP(const uint8_t *msg, size_t len)
    // If message won't fit, flush current buffer first
	// Error check message size
    // Append message to buffer
    // Send it if over UDP_SEND_INTERVAL
{
    if (udpBufferLen + len > UDP_SEND_MAX)
        flushUDP();
    if (len > UDP_SEND_MAX)
    {
        LOGE("tbESP32.ino: MESSAGE TOO BIG FOR UDP!! len=%d",len);
		return;
    }
    memcpy(udpBuffer + udpBufferLen, msg, len);
    udpBufferLen += len;
    if (millis() - lastSendMillis >= UDP_SEND_INTERVAL)
        flushUDP();
}


void loop()
{
    tb_esp32->loop();	// handle myIOT device

    if (tb_esp32->getConnectStatus() == IOT_CONNECT_STA)
    {
        if (!udp_started)
        {
            LOGI("Starting UDP");
            udp.begin(UDP_PORT);
            udp_started = 1;
            lastSendMillis = millis();
        }
        else
        {
            while (tbSerial.available())
            {
                uint8_t b = tbSerial.read();

				#if DEBUG_TB_SERIAL>1
					LOGV("serial=%d '%c'",b, b>32 && b<=0x7f ? b : ' ');
				#endif

                if (!inBinary)
                {
                    if (b == 0x02)
                    {
                        inBinary = true;
                        binLength = 0;
                        binReceived = 1;
                        binBuffer[0] = b;
                    }
                    else if (b == 0x0A)
                    {
                        textBuffer += (char)b;
						#if DEBUG_TB_SERIAL
							LOGV("got serial text: %s",textBuffer.c_str());
                        #endif
						addMessageToUDP((const uint8_t*)textBuffer.c_str(), textBuffer.length());
                        textBuffer = "";
                    }
                    else
                    {
                        textBuffer += (char)b;
                    }
                }
                else    // Binary mode
                {
                    binBuffer[binReceived++] = b;

                    if (binReceived == 3)
                    {
                        binLength = (uint16_t)binBuffer[1] | ((uint16_t)binBuffer[2] << 8);
						#if DEBUG_TB_SERIAL>1
							LOGV("got binlength=%d",binLength);
                        #endif
						if (binLength == 0)
                        {
							#if DEBUG_TB_SERIAL
								LOGV("got null binary buffer %d bytes",binReceived);
                            #endif
							addMessageToUDP(binBuffer, binReceived);
                            inBinary = false;
                            binLength = 0;
                            binReceived = 0;
                        }
                    }
                    else if (binReceived >= binLength + 3)
                    {
						#if DEBUG_TB_SERIAL
							LOGV("adding binLength(%d) binary buffer %d bytes",binLength,binReceived);
                        #endif
						addMessageToUDP(binBuffer, binReceived);
                        inBinary = false;
                        binLength = 0;
                        binReceived = 0;
                    }
                }
            }   // while tbSerial.available()

            // Time-based flush

            if (udpBufferLen > 0 && (millis() - lastSendMillis >= UDP_SEND_INTERVAL))
                flushUDP();

            // Non-blocking UDP receive.
            int packetSize = udp.parsePacket();
            if (packetSize)
            {
                std::vector<uint8_t> inBuf(packetSize+1);
                udp.read(inBuf.data(), packetSize);
                inBuf.push_back(0);
                if (DEBUG_UDP_TO_TEENSY)
                    LOGI("ESP32 received UDP packet(%d bytes)=%s", packetSize,inBuf.data());
                tbSerial.write(inBuf.data(), packetSize);
            }
        }
    }
    else if (udp_started)
    {
        LOGI("Stopping UDP");
        udp_started = 0;
    }
}