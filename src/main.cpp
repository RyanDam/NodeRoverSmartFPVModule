#include "main.h"

/** Camera class */
OV2640 cam;

void flashNoti(int num = 2) {
	for(int i=0; i < num; i++) {
		setLED(LED_NOTI);
		delay(100);
		setLED(0);
		delay(100);
	}
}

/** 
 * Called once after reboot/powerup
 */
void setup()
{
	// Start the serial connection
	Serial.begin(9600);

	// Serial.println("\n\n##################################");
	// Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
	// Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
	// Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
	// Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
	// Serial.println("##################################\n\n");

	//LED settings
	setupLEDPWM();
	flashNoti(1);

	// WiFiManager
	// Local intialization. Once its business is done, there is no need to keep it around
	WiFiManager wifiManager;
	
	// Uncomment and run it once, if you want to erase all the stored information
	// wifiManager.resetSettings();

	// fetches ssid and pass from eeprom and tries to connect
	// if it does not connect it starts an access point with the specified name
	// here  "AutoConnectAP"
	// and goes into a blocking loop awaiting configuration
	wifiManager.autoConnect("ESP_Cam", "12345678");
	
	// if you get here you have connected to the WiFi
	// Serial.println("Connected.");
	flashNoti();

	#ifdef ENABLE_WEBSERVER_FACE_DET
	esp32cam_aithinker_config.fb_count = 2;
	esp32cam_aithinker_config.grab_mode = CAMERA_GRAB_LATEST;
	esp32cam_aithinker_config.fb_location = CAMERA_FB_IN_PSRAM;
	esp32cam_aithinker_config.jpeg_quality = 20; /*!< Quality of JPEG output. 0-63 lower means higher quality  */
	if (enable_facedet) {
		esp32cam_aithinker_config.frame_size = STREAM_RES_DET;
		esp32cam_aithinker_config.pixel_format = STREAM_ENC_DET;
	} else {
		esp32cam_aithinker_config.frame_size = STREAM_RES_SRT;
		esp32cam_aithinker_config.pixel_format = STREAM_ENC_SRT;
	}
	#else
	esp32cam_aithinker_config.frame_size = FRAMESIZE_QVGA;
	esp32cam_aithinker_config.pixel_format = PIXFORMAT_JPEG;
	esp32cam_aithinker_config.jpeg_quality = 10; /*!< Quality of JPEG output. 0-63 lower means higher quality  */
	esp32cam_aithinker_config.fb_location = CAMERA_FB_IN_PSRAM;
	esp32cam_aithinker_config.fb_count = 2;
	#endif

	cam.init(esp32cam_aithinker_config);
	flashNoti();

	#ifdef ENABLE_WEBSERVER_FACE_DET
	chip_total_heap = ESP.getHeapSize();
	chip_total_psram = ESP.getPsramSize();
	init_buffer();
	initVisionTask();

	#ifdef ENABLE_ROBOT_CONTROL
		initRobotTask();
	#endif
	#endif

	initCameraTask();

#ifdef ENABLE_WEBSERVER_CONTROL
	// Initialize the HTTP web stream server
	initWebControl();
#endif
#ifdef ENABLE_WEBSERVER_STREAM
	// Initialize the HTTP web stream server
	initWebStream();
#endif
}

void loop()
{
	if (otaStarted)
	{
		ArduinoOTA.handle();
	}
	//Nothing else to do here
	delay(100);
}


