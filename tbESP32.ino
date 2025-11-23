
#include "tbESP32.h"


//------------------------------
// myIOT setup
//------------------------------

static valueIdType dash_items[] = {
	ID_STATUS,
    0
};


static valueIdType config_items[] = {
    0
};


const valDescriptor tbESP32_values[] =
{
    {ID_DEVICE_NAME,		VALUE_TYPE_STRING,  VALUE_STORE_PREF,     	VALUE_STYLE_REQUIRED,   NULL,   	NULL,   FRIDGE_CONTROLLER },
		// override base class element
	{ID_STATUS,      		VALUE_TYPE_STRING,	VALUE_STORE_PUB,		VALUE_STYLE_READONLY,	(void *) &Fridge::_status_str,   	NULL,	},
};


#define NUM_TBESP32_VALUES (sizeof(tbESP32_values)/sizeof(valDescriptor))


String  Fridge::_status_str;


//--------------------------------
// main
//--------------------------------


void setup()
{
    Serial.begin(MY_IOT_ESP32_CORE == 3 ? 115200 : 921600);
    delay(1000);

    LOGU("tbESP32.ino setup() started",0);

	#if WITH_PWM
		ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
		ledcAttachPin(PIN_PUMP_PWM, PWM_CHANNEL);
		ledcWrite(PWM_CHANNEL, 0);
	#endif

    Fridge::setDeviceType(FRIDGE_CONTROLLER);
    Fridge::setDeviceVersion(FRIDGE_CONTROLLER_VERSION);
    Fridge::setDeviceUrl(FRIDGE_CONTROLLER_URL);

    LOGU("");
    fridge = new Fridge();
    fridge->addValues(fridge_values,NUM_FRIDGE_VALUES);
	fridge->setTabLayouts(dash_items,config_items);
	fridge->addDerivedToolTips(fridge_tooltips);
    LOGU("fridgeController.ino setup() started on core(%d)",xPortGetCoreID());

    fridge->setup();

	// clear indicators

	digitalWrite(LED_FAN_ON,0);
	digitalWrite(LED_POWER_ON,0);
	digitalWrite(LED_DIODE_ON,0);
	digitalWrite(LED_COMPRESS_ON,0);

	setPixel(PIXEL_SYSTEM,0);
	setPixel(PIXEL_STATE,0);
	setPixel(PIXEL_ERROR,0);
	showPixels();

    LOGU("tbESP32.ino setup() finished",0);
}



void loop()
{
    fridge->loop();
}
