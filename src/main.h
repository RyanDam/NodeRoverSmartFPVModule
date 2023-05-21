// WiFi stuff
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

// OTA stuff
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Camera stuff
#include "OV2640.h"
#include "OV2640Streamer.h"

#define ENABLE_WEBSERVER_CONTROL
#define ENABLE_WEBSERVER_STREAM
#define ENABLE_WEBSERVER_FACE_DET
// #define ENABLE_WEBSERVER_FACE_LAND
#define ENABLE_ROBOT_CONTROL

// Vision stuff
#ifdef ENABLE_WEBSERVER_FACE_DET
#include "fb_gfx.h"
#include "esp_http_server.h"
#include "human_face_detect_msr01.hpp"
#include "bencher.h"
#include <ArduinoJson.h>
#ifdef ENABLE_WEBSERVER_FACE_LAND
#include "human_face_detect_mnp01.hpp"
#endif

#define STREAM_RES_DET FRAMESIZE_QVGA
#define STREAM_ENC_DET PIXFORMAT_JPEG
#define STREAM_RES_SRT FRAMESIZE_QVGA
#define STREAM_ENC_SRT PIXFORMAT_JPEG

#define FACE_DET_BUF_WIDTH 320
#define FACE_DET_BUF_HEIGHT 240
#define FACE_DET_BUF_CHANNEL 3
#define FACE_DET_BUF_BPP 2
#define FACE_DET_MAX_NUM 1
#define FACE_DET_SCORE_THRES 0.05

typedef struct
{
    WiFiClient *client;
    size_t len;
} jpg_chunking_t;

size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len);
void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id);
void draw_fps(fb_data_t *fb, int fps);

void initVisionTask(void);
void stopVisionTask(void);
void setNeedVisionRunning(void);

extern uint8_t* camBuffer;
extern uint8_t* frameBuffer;
extern size_t camBufferSize;
void init_buffer();

extern uint32_t chip_total_heap;
extern uint32_t chip_total_psram;

extern boolean enable_facedet;
extern boolean cam_updating;
extern boolean vision_running;
extern boolean cam_buffer_copying;
extern SemaphoreHandle_t cam_buffer_mutex;
extern SemaphoreHandle_t cam_capture_mutex;

extern Bencher bench_read;
extern Bencher bench_decode;
extern Bencher bench_detect;
#ifdef ENABLE_WEBSERVER_FACE_LAND
extern Bencher bench_align;
#endif
extern Bencher bench_predict;
extern Bencher bench_draw;
extern Bencher bench_encode;
extern Bencher bench_stream;
extern Bencher bench_total;

extern dl::detect::result_t currentDetResult[];

// Robot Stuff
#ifdef ENABLE_ROBOT_CONTROL

enum RobotMotorCmd {
    MOTOR_FORWARD, 
    MOTOR_BACKWARD, 
    MOTOR_LEFT, 
    MOTOR_RIGHT,
    MOTOR_STOP = 999
};
enum RobotArmCmd {
    SERVO_UP, 
    SERVO_DOWN, 
    SERVO_LEFT, 
    SERVO_RIGHT, 
    SERVO_RESET, 
    SERVO_ULEFT, 
    SERVO_URIGHT, 
    SERVO_DLEFT, 
    SERVO_DRIGHT,
    SERVO_STOP = 999
};

struct RobotStats {
    float batteryVoltage = -1;
    float maxHeap = -1;
    float freeHeap = -1;
};

extern RobotStats robotStat;

void initRobotTask(void);
void stopRobotTask(void);
void sendRobotMotorCmd(RobotMotorCmd cmd);
void sendRobotArmCmd(RobotArmCmd cmd);
bool updateCameraArm();
void setNeedRobotRunning(void);

#endif

#endif

#define LED_INIT_BRIGHTNESS 0
#define LED_FREQ 5000 // PWM settings
#define LED_CHANNEL 15 // camera uses timer1
#define LED_RES 8 // resolution (8 = from 0 to 255)
#define LED_NOTI 1
#define LED_FLASH 128

void setupLEDPWM();
void setLED(byte brightness);

// Camera class
extern OV2640 cam;

void initCameraTask(void);
void stopCameraTask(void);

#ifdef ENABLE_WEBSERVER_FACE_DET
// Vision class
extern HumanFaceDetectMSR01 s1;
#ifdef ENABLE_WEBSERVER_FACE_LAND
extern HumanFaceDetectMNP01 s2;
#endif
#endif

// Web server stuff
#ifdef ENABLE_WEBSERVER_CONTROL
void initWebControl(void);
void stopWebControl(void);
#endif

#ifdef ENABLE_WEBSERVER_STREAM
void initWebStream(void);
void stopWebStream(void);
#endif

// OTA stuff
/** Function declarations */
void enableOTA(void);
void resetDevice(void);
void startOTA(void);
void stopOTA(void);
extern boolean otaStarted;
