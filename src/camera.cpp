#include "main.h"

uint8_t* camBuffer;
size_t camBufferSize;
boolean cam_updating = false;
SemaphoreHandle_t cam_buffer_mutex = xSemaphoreCreateMutex();
SemaphoreHandle_t cam_capture_mutex = xSemaphoreCreateMutex();

void cameraTask(void *pvParameters);
TaskHandle_t cameraTaskHandler;
boolean stopCameraFlag = false;

void initCameraTask(void) {
    // Create the task for the Vision and run it in core 0 (others tasks use core 1)
	xTaskCreatePinnedToCore(cameraTask, "CAMERA", 4096, NULL, 1, &cameraTaskHandler, 0);

	if (cameraTaskHandler == NULL)
	{
		// Serial.println("Create Vision task failed");
	}
	else
	{
		// Serial.println("Vision task up and running");
	}
}

void stopCameraTask(void) {
    stopCameraFlag = true;
}   

void cameraTask(void *pvParameters) {
    uint32_t camera_update_last_ = 0, camera_update_period_ = 63, now_ = 0; // 20 fps

    while (!stopCameraFlag) {

        while (cam_updating) {
            delay(10);
        }

        now_ =  millis();
		if(now_ - camera_update_last_ > camera_update_period_)
    	{
            camera_update_last_ = now_;

            xSemaphoreTake(cam_capture_mutex, portMAX_DELAY); // enter critical section
            bench_read.begin();
            cam.run();
            bench_read.end();
            xSemaphoreGive(cam_capture_mutex); // exit critical section

            // xSemaphoreTake(cam_buffer_mutex, portMAX_DELAY); // enter critical section
            if (enable_facedet && !vision_running) {
                camBufferSize = cam.getSize();
                for(size_t i = 0; i < camBufferSize; i++){
                    camBuffer[i] = cam.getfb()[i];
                }
                setNeedVisionRunning();
            }
            // xSemaphoreGive(cam_buffer_mutex); // exit critical section
        } else {
            delay(10);
        }
    }

    if (stopCameraFlag)
	{
		// Delete this task
		vTaskDelete(NULL);
	}

	delay(10);
}

