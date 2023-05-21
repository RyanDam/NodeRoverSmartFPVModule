#include "main.h"
#include <ArduinoJson.h>

#ifdef ENABLE_ROBOT_CONTROL

RobotStats robotStat;
boolean robot_need_run = false;

void robotTask(void *pvParameters);
TaskHandle_t robotTaskHandler;
boolean stopRobotFlag = false;

void readSerial();

void initRobotTask(void) {
    // Create the task for the Vision and run it in core 0 (others tasks use core 1)
	xTaskCreatePinnedToCore(robotTask, "ROBOT", 4096, NULL, 1, &robotTaskHandler, 0);

	if (robotTaskHandler == NULL)
	{
		// Serial.println("Create Vision task failed");
	}
	else
	{
		// Serial.println("Vision task up and running");
	}
}

void stopRobotTask(void) {
    stopRobotFlag = true;
}

void robotTask(void *pvParameters) {
    long duration_ = 0;
    long start_ = 0;
    const long minSleep_ = 10;

    while (!stopRobotFlag) {
        start_ = millis();

        updateCameraArm();

        readSerial();

        duration_ = millis() - start_;
        if (duration_ < minSleep_) delay(minSleep_ - duration_);
    }

    if (stopRobotFlag)
	{
		// Delete this task
		vTaskDelete(NULL);
	}

	delay(10);
}

void readSerial() {
    // Check if the other Arduino is transmitting
    if (Serial.available()) {
        // Allocate the JSON document
        // This one must be bigger than the sender's because it must store the strings
        StaticJsonDocument<256> report;

        // Read the JSON document from the "link" serial port
        DeserializationError err = deserializeJson(report, Serial);

        if (err == DeserializationError::Ok) {
            robotStat.batteryVoltage = report["batt"].as<float>();
            robotStat.freeHeap = report["heap_free"].as<uint32_t>();
            robotStat.maxHeap = report["heap_max"].as<uint32_t>();
        } else {
            // ERR
            // Flush all bytes in the "link" serial port buffer
            while (Serial.available() > 0) Serial.read();
        }
    }
}

void sendRobotMotorCmd(RobotMotorCmd cmd) {

    // Create the JSON document
    StaticJsonDocument<256> command;
    command["mode"] = "r";
    command["cmd"] = (long)cmd;

    serializeJson(command, Serial);

    // Serial.write('r');
    // switch (cmd)
    // {
    // case MOTOR_STOP: {
    //     Serial.write('9');
    //     Serial.write('9');
    //     return;
    // }
    // case MOTOR_FORWARD:
    //     Serial.write('0');
    //     break;
    // case MOTOR_BACKWARD:
    //     Serial.write('1');
    //     break;
    // case MOTOR_LEFT:
    //     Serial.write('2');
    //     break;
    // case MOTOR_RIGHT:
    //     Serial.write('3');
    //     break;
    // }
    // Serial.write('0');
    // Serial.write('\n');
}

void sendRobotArmCmd(RobotArmCmd cmd) {
    // Create the JSON document
    StaticJsonDocument<256> command;
    command["mode"] = "c";
    command["cmd"] = (long)cmd;

    serializeJson(command, Serial);

    // Serial.write('c');
    // switch (cmd)
    // {
    // case SERVO_STOP: {
    //     Serial.write('9');
    //     Serial.write('9');
    //     return;
    // }
    // case SERVO_UP:
    //     Serial.write('0');
    //     break;
    // case SERVO_DOWN:
    //     Serial.write('1');
    //     break;
    // case SERVO_LEFT:
    //     Serial.write('2');
    //     break;
    // case SERVO_RIGHT:
    //     Serial.write('3');
    //     break;
    // case SERVO_RESET:
    //     Serial.write('4');
    //     break;
    // case SERVO_ULEFT:
    //     Serial.write('5');
    //     break;
    // case SERVO_URIGHT:
    //     Serial.write('6');
    //     break;
    // case SERVO_DLEFT:
    //     Serial.write('7');
    //     break;
    // case SERVO_DRIGHT:
    //     Serial.write('8');
    //     break;
    // }
    // Serial.write('0');
    // Serial.write('\n');
}

bool updateCameraArm() {
    if (!robot_need_run) return false;
    robot_need_run = false;

    int x, y, w, h, cx, cy;
    int minx = 130, maxx = 190;
    int miny = 100, maxy = 140;
    int i = 0;

    boolean camera_moved = false;
    int movex = 0;
    int movey = 0;
    for (int i = 0; i < FACE_DET_MAX_NUM; i++) {
        if (currentDetResult[i].score < FACE_DET_SCORE_THRES) continue;

        cx = (currentDetResult[i].box[0] + currentDetResult[i].box[2]) / 2;
        cy = (currentDetResult[i].box[1] + currentDetResult[i].box[3]) / 2;

        if (cx < minx) {sendRobotArmCmd(SERVO_LEFT); camera_moved = true; movex = minx-cx;}
        else if (cx > maxx) {sendRobotArmCmd(SERVO_RIGHT); camera_moved = true; movex = cx-maxx;}
        if (cy < miny) {sendRobotArmCmd(SERVO_UP); camera_moved = true; movey = miny - cy;}
        else if (cy > maxy) {sendRobotArmCmd(SERVO_DOWN); camera_moved = true; movey = cy - maxy;}
    }

    if (camera_moved) {
        int targetDuration = max(movex, movey)*1.2;
        delay(targetDuration);
        sendRobotArmCmd(SERVO_STOP);
    }

    return camera_moved;
}

void setNeedRobotRunning(void) {
    robot_need_run = true;
}

#endif

