#include "main.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/micro/micro_log.h"
// #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
// #include "tensorflow/lite/schema/schema_generated.h"

#ifdef ENABLE_WEBSERVER_FACE_DET

#define FACE_COLOR_WHITE 0x00FFFFFF
#define FACE_COLOR_BLACK 0x00000000
#define FACE_COLOR_RED 0x000000FF
#define FACE_COLOR_GREEN 0x0000FF00
#define FACE_COLOR_BLUE 0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

HumanFaceDetectMSR01 s1(FACE_DET_SCORE_THRES, 0.5F, FACE_DET_MAX_NUM, 0.2F);
#ifdef ENABLE_WEBSERVER_FACE_LAND
HumanFaceDetectMNP01 s2(0.1F, 0.3F, 5);
#endif

uint8_t* frameBuffer;

uint32_t chip_total_heap;
uint32_t chip_total_psram;

boolean enable_facedet = false;

boolean vision_running = false;
boolean vision_need_run = false;

Bencher bench_read;
Bencher bench_decode;
Bencher bench_detect;
#ifdef ENABLE_WEBSERVER_FACE_LAND
Bencher bench_align;
#endif
Bencher bench_predict;
Bencher bench_draw;
Bencher bench_encode;
Bencher bench_stream;
Bencher bench_total;

dl::detect::result_t currentDetResult[FACE_DET_MAX_NUM];

void visionTask(void *pvParameters);
TaskHandle_t visionTaskHandler;
boolean stopVisionFlag = false;

void initVisionTask(void) {
    // Create the task for the Vision and run it in core 0 (others tasks use core 1)
	xTaskCreatePinnedToCore(visionTask, "VISION", 4096, NULL, 1, &visionTaskHandler, 1);

	if (visionTaskHandler == NULL)
	{
		// Serial.println("Create Vision task failed");
	}
	else
	{
		// Serial.println("Vision task up and running");
	}
}

void stopVisionTask(void) {
    stopVisionFlag = true;
}

void setNeedVisionRunning(void) {
    vision_need_run = true;
}

void process_det_result(std::list<dl::detect::result_t> *results);

void visionTask(void *pvParameters) {

    // init results holder
    for (int i = 0; i < FACE_DET_MAX_NUM; i++) {
        for (int y = 0; y < 4; y++) {
		    currentDetResult[i].box.push_back(0);
        }
        for (int y = 0; y < 10; y++) {
		    currentDetResult[i].keypoint.push_back(0);
        }
		currentDetResult[i].score = 0.f;
        currentDetResult[i].category = 0;
	}

    // init
    fb_data_t rfb;
    rfb.width = cam.getWidth();
    rfb.height = cam.getHeight();
    rfb.data = frameBuffer;
    rfb.bytes_per_pixel = 2;
    rfb.format = FB_RGB565;	

    int fps_ = 0;

    while (!stopVisionFlag)
	{
        while (cam_updating) {
            delay(10);
        }

        // main loop
        if (enable_facedet && vision_need_run) {
            vision_need_run = false;
            vision_running = true;

            bench_total.begin();

            // xSemaphoreTake(cam_buffer_mutex, portMAX_DELAY); // enter critical section
            if (cam.getPixelFormat() == PIXFORMAT_RGB565) {
                for(size_t i = 0; i < camBufferSize; i++){
                    frameBuffer[i] = camBuffer[i];
                }
            } else {
                bench_decode.begin();
                jpg2rgb565((const uint8_t*)camBuffer, camBufferSize, frameBuffer, JPG_SCALE_NONE);

                // swap the byte order and the image looks ok
                size_t i;
                for(i = 0; i < FACE_DET_BUF_WIDTH*FACE_DET_BUF_HEIGHT*FACE_DET_BUF_BPP; i+=2){
                    const uint8_t tmp = frameBuffer[i];
                    frameBuffer[i] = frameBuffer[i+1];
                    frameBuffer[i+1] = tmp;
                }

                bench_decode.end();
            }
            // xSemaphoreGive(cam_buffer_mutex); // exit critical section

            if (enable_facedet) {
                bench_predict.begin();
                #ifdef ENABLE_WEBSERVER_FACE_LAND
                bench_detect.begin();
                std::list<dl::detect::result_t> &candidates = s1.infer((uint16_t *)rfb.data, {cam.getHeight(), cam.getWidth(), 3});
                bench_detect.end();
                bench_align.begin();
                std::list<dl::detect::result_t> &results = s2.infer((uint16_t *)rfb.data, {cam.getHeight(), cam.getWidth(), 3}, candidates);
                bench_align.end();
                #else
                bench_detect.begin();
                std::list<dl::detect::result_t> &results = s1.infer((uint16_t *)rfb.data, {cam.getHeight(), cam.getWidth(), 3});
                bench_detect.end();
                #endif
                bench_predict.end();
                
                process_det_result(&results);

                if (results.size() > 0) {
                    bench_draw.begin();
                    draw_face_boxes(&rfb, &results, 0);
                    bench_draw.end();
                }
            }

            vision_running = false;

            bench_total.end();

        } else {
            delay(10);
        }

        
	}

    if (stopVisionFlag)
	{
		// Delete this task
		vTaskDelete(NULL);
	}

	delay(10);
}

void init_buffer() {
    camBuffer = (uint8_t*) ps_malloc(FACE_DET_BUF_WIDTH*FACE_DET_BUF_HEIGHT*FACE_DET_BUF_CHANNEL);
    frameBuffer = (uint8_t*) ps_malloc(FACE_DET_BUF_WIDTH*FACE_DET_BUF_HEIGHT*FACE_DET_BUF_BPP);
}

size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }

	j->client->write((const char *)data, len);

    j->len += len;
    return len;
}

void draw_fps(fb_data_t *fb, int fps) {
    fb_gfx_printf(fb, 5, 5, FACE_COLOR_BLACK, "FPS: %d", fps);
}

void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0)
    {
        color = FACE_COLOR_RED;
    }
    else if (face_id > 0)
    {
        color = FACE_COLOR_GREEN;
    }
    if(fb->bytes_per_pixel == 2){
        //color = ((color >> 8) & 0xF800) | ((color >> 3) & 0x07E0) | (color & 0x001F);
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++)
    {
        // rectangle box
        x = (int)prediction->box[0];
        y = (int)prediction->box[1];
        w = (int)prediction->box[2] - x + 1;
        h = (int)prediction->box[3] - y + 1;
        if((x + w) > fb->width){
            w = fb->width - x;
        }
        if((y + h) > fb->height){
            h = fb->height - y;
        }
        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
#ifdef ENABLE_WEBSERVER_FACE_LAND
        // landmarks (left eye, mouth left, nose, right eye, mouth right)
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)prediction->keypoint[j];
            y0 = (int)prediction->keypoint[j+1];
            fb_gfx_fillRect(fb, x0, y0, 3, 3, FACE_COLOR_BLUE);
        }
#endif
    }
}
#endif

void process_det_result(std::list<dl::detect::result_t> *results) {
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++)
    {
        currentDetResult[i].box = prediction->box;
        currentDetResult[i].keypoint = prediction->keypoint;
        currentDetResult[i].score = prediction->score;
        currentDetResult[i].category = prediction->category;
    }
    for (int y = i; y < FACE_DET_MAX_NUM; y++) {
        currentDetResult[i].score = 0.f;
    }

    #ifdef ENABLE_ROBOT_CONTROL
    setNeedRobotRunning();
    #endif
}