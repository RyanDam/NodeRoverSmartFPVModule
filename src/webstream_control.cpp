#include "main.h"
#ifdef ENABLE_WEBSERVER_CONTROL
/** Web server class */
WebServer server_control(80);

/** Forward dedclaration of the task handling browser requests */
void webControlTask(void *pvParameters);

/** Task handle of the web task */
TaskHandle_t webControlTaskHandler;

/** Flag to stop the web server_control */
boolean stopWebControlFlag = false;

/** Web request function forward declarations */
void handle_jpg_stream2(void);
void handle_jpg2(void);
void handle_ledon2(void);
void handle_ledoff2(void);
void handle_start_ota2(void);
#ifdef ENABLE_WEBSERVER_FACE_DET
void handle_fd_on2(void);
void handle_fd_off2(void);
void handle_report2(void);
#endif
void handleNotFound2();

/**
 * Initialize the web stream server_control by starting the handler task
 */
void initWebControl(void)
{
	// Create the task for the web server_control and run it in core 0 (others tasks use core 1)
	xTaskCreatePinnedToCore(webControlTask, "WEB", 4096, NULL, 1, &webControlTaskHandler, 1);

	if (webControlTaskHandler == NULL)
	{
		// Serial.println("Create WebControl task failed");
	}
	else
	{
		// Serial.println("WebControl task up and running");
	}
}

/**
 * Called to stop the web server_control task, needed for OTA
 * to avoid OTA timeout error
 */
void stopWebControl(void)
{
	stopWebControlFlag = true;
}

/**
 * The task that handles web server_control connections
 * Starts the web server_control
 * Handles requests in an endless loop
 * until a stop request is received because OTA
 * starts
 */
void webControlTask(void *pvParameters)
{
  	server_control.enableCORS(true); //This is the magic
	// Set the function to handle stream requests
	server_control.on("/stream", HTTP_GET, handle_jpg_stream2);
	// Set the function to handle single picture requests
	server_control.on("/jpg", HTTP_GET, handle_jpg2);
	server_control.on("/ledon", HTTP_GET, handle_ledon2);
	server_control.on("/ledoff", HTTP_GET, handle_ledoff2);
	server_control.on("/start_ota", HTTP_GET, handle_start_ota2);
	#ifdef ENABLE_WEBSERVER_FACE_DET
	server_control.on("/fdon", HTTP_GET, handle_fd_on2);
	server_control.on("/fdoff", HTTP_GET, handle_fd_off2);
	server_control.on("/report", HTTP_GET, handle_report2);
	#endif
	// Set the function to handle other requests
	server_control.onNotFound(handleNotFound2);
	// Start the web server_control
	server_control.begin();

	while (!stopWebControlFlag)
	{
		// Check if the server_control has clients
		server_control.handleClient();
	}
	if (stopWebControlFlag)
	{
		// User requested web server_control stop
		server_control.close();
		// Delete this task
		vTaskDelete(NULL);
	}
	delay(10);
}

/**
 * Handle web stream requests
 * redirect to ip:81/stream
 */

void handle_jpg_stream2(void)
{
	IPAddress ip = WiFi.localIP();
	server_control.sendHeader("Location", String("http://")+ ip.toString()+ String(":81/stream"));
	server_control.send(302, "text/plain", "");
	server_control.client().stop();
}

/**
 * Handle single picture requests
 * Gets the latest picture from the camera
 * and sends it to the web client
 */
void handle_jpg2(void)
{
	WiFiClient thisClient = server_control.client();

	cam.run();
	if (!thisClient.connected())
	{
		return;
	}
	String response = "HTTP/1.1 200 OK\r\n";
	response += "Content-disposition: inline; filename=capture.jpg\r\n";
	response += "Content-type: image/jpeg\r\n\r\n";
	server_control.sendContent(response);
	thisClient.write((char *)cam.getfb(), cam.getSize());

}
void handle_ledon2(void)
{
	setLED(LED_FLASH);
	String message = "Done";
	message += "\n";
	server_control.send(200, "text/plain", message);
}
void handle_ledoff2(void)
{
	setLED(0);
	String message = "Done";
	message += "\n";
	server_control.send(200, "text/plain", message);
}

#ifdef ENABLE_WEBSERVER_FACE_DET
void handle_fd_on2(void) {
	cam_updating = true;

	// esp_camera_deinit();
	// delay(100);
	// esp32cam_aithinker_config.frame_size = STREAM_RES_DET;
	// esp32cam_aithinker_config.pixel_format = STREAM_ENC_DET;
	// cam.init(esp32cam_aithinker_config);

	enable_facedet = true;
	cam_updating = false;

	String message = "Done";
	message += "\n";
	server_control.send(200, "text/plain", message);
}

void handle_fd_off2(void) {
	cam_updating = true;

	// esp_camera_deinit();
	// delay(100);
	// esp32cam_aithinker_config.frame_size = STREAM_RES_SRT;
	// esp32cam_aithinker_config.pixel_format = STREAM_ENC_SRT;
	// cam.init(esp32cam_aithinker_config);

	enable_facedet = false;
	cam_updating = false;

	String message = "Done";
	message += "\n";
	server_control.send(200, "text/plain", message);
}

void handle_report2(void) {
	DynamicJsonDocument benchReport(1024);

	benchReport["total"] = bench_total.avg();
	benchReport["read"] = bench_read.avg();
	benchReport["decode"] = bench_decode.avg();
	benchReport["detect"] = bench_detect.avg();
#ifdef ENABLE_WEBSERVER_FACE_LAND
	benchReport["align"] = bench_align.avg();
#endif
	benchReport["predict"] = bench_predict.avg();
	benchReport["draw"] = bench_draw.avg();
	benchReport["endcode"] = bench_encode.avg();
	benchReport["stream"] = bench_stream.avg();
#ifdef ENABLE_ROBOT_CONTROL
	benchReport["robot"]["batteryVoltage"] = robotStat.batteryVoltage;
	benchReport["robot"]["heap"]["free"] = robotStat.freeHeap;
	benchReport["robot"]["heap"]["total"] = robotStat.maxHeap;
#endif
	benchReport["temp"] = temperatureRead();

	benchReport["heap"]["total"] = chip_total_heap;
	benchReport["heap"]["free"] = ESP.getFreeHeap();
	
	benchReport["psram"]["total"] = chip_total_psram;
	benchReport["psram"]["free"] = ESP.getFreePsram();

	for (int i = 0; i < FACE_DET_MAX_NUM; i++) {
		for (int y = 0; y < 4; y++) {
			benchReport["faces"][i]["box"][y] =  currentDetResult[i].box[y];
        }
#ifdef ENABLE_WEBSERVER_FACE_LAND
		for (int y = 0; y < 10; y++) {
			benchReport["faces"][i]["keypoint"][y] =  currentDetResult[i].keypoint[y];
		}
#endif
		benchReport["faces"][i]["score"] = enable_facedet ? currentDetResult[i].score : 0.f;
		benchReport["faces"][i]["cat"] =  currentDetResult[i].category;
	}

	String response;
	serializeJsonPretty(benchReport, response);
	
	String message = "Done";
	message += "\n";
	server_control.send(200, "application/json", response);
}

#endif

void handle_start_ota2(void)
{
	String message = "Going into OTA mode";
	message += "\n";
	server_control.send(200, "text/plain", message);
	enableOTA();
}

/**
 * Handle any other request from the web client
 */
void handleNotFound2()
{
	IPAddress ip = WiFi.localIP();
	String message = "Browser Stream Link: http://";
	message += ip.toString();
	message += "81:/stream\n";
	message += "Browser Single Picture Link: http://";
	message += ip.toString();
	message += "/jpg\n";
	message += "Switch LED ON: http://";
	message += ip.toString();
	message += "/ledon\n";
	message += "Switch LED OFF: http://";
	message += ip.toString();
	message += "/ledoff\n";
	message += "Start OTA mode: http://";
	message += ip.toString();
	message += "/start_ota\n";
	message += "\n";
	server_control.send(200, "text/plain", message);
}
#endif
