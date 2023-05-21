#include "main.h"
#ifdef ENABLE_WEBSERVER_STREAM

/** Web server class */
WebServer server_stream(81);

/** Forward dedclaration of the task handling browser requests */
void webStreamTask(void *pvParameters);

/** Task handle of the web task */
TaskHandle_t webStreamTaskHandler;

/** Flag to stop the web server_stream */
boolean stopWebStreamFlag = false;

/** Web request function forward declarations */
void handle_jpg_stream(void);
void handle_jpg(void);
void handle_ledon(void);
void handle_ledoff(void);
void handle_start_ota(void);
#ifdef ENABLE_WEBSERVER_FACE_DET
void handle_fd_on(void);
void handle_fd_off(void);
void handle_report(void);
#endif
void handleNotFound();

/**
 * Initialize the web stream server_stream by starting the handler task
 */
void initWebStream(void)
{

	// Create the task for the web server_stream and run it in core 0 (others tasks use core 1)
	xTaskCreatePinnedToCore(webStreamTask, "WEB", 4096, NULL, 1, &webStreamTaskHandler, 0);

	if (webStreamTaskHandler == NULL)
	{
		// Serial.println("Create WebStream task failed");
	}
	else
	{
		// Serial.println("WebStream task up and running");
	}
}

/**
 * Called to stop the web server_stream task, needed for OTA
 * to avoid OTA timeout error
 */
void stopWebStream(void)
{
	stopWebStreamFlag = true;
}

/**
 * The task that handles web server_stream connections
 * Starts the web server_stream
 * Handles requests in an endless loop
 * until a stop request is received because OTA
 * starts
 */
void webStreamTask(void *pvParameters)
{
  	server_stream.enableCORS(true); //This is the magic
	// Set the function to handle stream requests
	server_stream.on("/stream", HTTP_GET, handle_jpg_stream);
	// Set the function to handle single picture requests
	server_stream.on("/jpg", HTTP_GET, handle_jpg);
	server_stream.on("/ledon", HTTP_GET, handle_ledon);
	server_stream.on("/ledoff", HTTP_GET, handle_ledoff);
	server_stream.on("/start_ota", HTTP_GET, handle_start_ota);
	#ifdef ENABLE_WEBSERVER_FACE_DET
	server_stream.on("/fdon", HTTP_GET, handle_fd_on);
	server_stream.on("/fdoff", HTTP_GET, handle_fd_off);
	server_stream.on("/report", HTTP_GET, handle_report);
	#endif
	// Set the function to handle other requests
	server_stream.onNotFound(handleNotFound);
	// Start the web server_stream
	server_stream.begin();

	while (!stopWebStreamFlag)
	{
		// Check if the server_stream has clients
		server_stream.handleClient();
	}
	if (stopWebStreamFlag)
	{
		// User requested web server_stream stop
		server_stream.close();
		// Delete this task
		vTaskDelete(NULL);
	}
	delay(10);
}

/**
 * Handle web stream requests
 * Gives a first response to prepare the streaming
 * Then runs in a loop to update the web content
 * every time a new frame is available
 */
void handle_jpg_stream(void)
{
	WiFiClient thisClient = server_stream.client();
	String response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
	server_stream.sendContent(response);

	uint32_t stream_update_last_ = 0, stream_update_period_ = 63, now_ = 0; // 20 fps

	while (1)
	{
		if (!thisClient.connected() || otaStarted)
		{
			break;
		}

		while (cam_updating) {
			delay(10);
		}

		now_ =  millis();
		if(now_ - stream_update_last_ > stream_update_period_)
    	{
			stream_update_last_ = now_;

			#ifdef ENABLE_WEBSERVER_FACE_DET // buffer is rgb565

			xSemaphoreTake(cam_capture_mutex, portMAX_DELAY); // enter critical section
			if (cam.getPixelFormat() == PIXFORMAT_RGB565) {
				bench_stream.begin();
				jpg_chunking_t jchunk = {&thisClient, 0};
				response = "--frame\r\n";
				response += "Content-Type: image/jpeg\r\n\r\n";
				server_stream.sendContent(response);
				fmt2jpg_cb(cam.getfb(), cam.getSize(), cam.getWidth(), cam.getHeight(), PIXFORMAT_RGB565, (uint8_t) 10, jpg_encode_stream, &jchunk);
				server_stream.sendContent("\r\n");
				bench_stream.end();
			} else { // jpeg
				bench_stream.begin();
				response = "--frame\r\n";
				response += "Content-Type: image/jpeg\r\n\r\n";
				server_stream.sendContent(response);
				thisClient.write((char *)cam.getfb(), cam.getSize());
				server_stream.sendContent("\r\n");
				bench_stream.end();
			}
			xSemaphoreGive(cam_capture_mutex); // exit critical section
			
			#else // buffer is jpeg already

			cam.run();
			thisClient.write((char *)cam.getfb(), cam.getSize());
			server_stream.sendContent("\r\n");

			#endif

		} else {
			delay(10);
		}
	}
}

/**
 * Handle single picture requests
 * redirect to the other stream
 */
void handle_jpg(void)
{
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/jpg"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();

}
void handle_ledon(void)
{
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/ledon"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}
void handle_ledoff(void)
{
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/ledoff"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}

#ifdef ENABLE_WEBSERVER_FACE_DET
void handle_fd_on(void) {
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/fdon"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}

void handle_fd_off(void) {
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/fdoff"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}

void handle_report(void) {
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/report"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}

#endif

void handle_start_ota(void)
{
	IPAddress ip = WiFi.localIP();
	server_stream.sendHeader("Location", String("http://")+ ip.toString()+ String(":80/start_ota"));
	server_stream.send(302, "text/plain", "");
	server_stream.client().stop();
}

/**
 * Handle any other request from the web client
 */
void handleNotFound()
{
	IPAddress ip = WiFi.localIP();
	String message = "Browser Stream Link: http://";
	message += ip.toString();
	message += ":81/stream\n";
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
	server_stream.send(200, "text/plain", message);
}
#endif