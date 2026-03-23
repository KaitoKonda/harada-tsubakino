/*
 * OTOS + HMC6343 serial publisher for Arduino Nano ESP32
 *
 * Control input format:
 * START
 * STOP
 * RESET
 * PING
 *
 * Serial output format:
 * OTOS,<millis>,<x_m>,<y_m>,<heading_rad>
 * HMC,<millis>,<roll_rad>,<pitch_rad>,<heading_rad>,<ax_mps2>,<ay_mps2>,<az_mps2>
 */

#include <Wire.h>

#include <SparkFun_Qwiic_OTOS_Arduino_Library.h>
#include <SFE_HMC6343.h>

namespace
{
const unsigned long kOtosPublishIntervalMs = 20;      // 50 Hz
const unsigned long kCompassPublishIntervalMs = 200;  // 5 Hz
const unsigned long kStatusIntervalMs = 1000;
const unsigned long kBridgeTimeoutMs = 1000;
const float kDegToRad = 0.01745329252f;
const float kGToMps2 = 9.80665f;

QwiicOTOS otos;
SFE_HMC6343 compass;

bool otos_ready = false;
bool compass_ready = false;
bool streaming_enabled = false;
unsigned long last_otos_publish_ms = 0;
unsigned long last_compass_publish_ms = 0;
unsigned long last_status_ms = 0;
unsigned long last_bridge_seen_ms = 0;
String command_buffer;
}

void resetSensors()
{
  if (!otos_ready)
  {
    otos_ready = otos.begin();
    if (otos_ready)
    {
      otos.setLinearUnit(kSfeOtosLinearUnitMeters);
      otos.setAngularUnit(kSfeOtosAngularUnitRadians);
    }
  }

  compass_ready = compass.init();

  if (otos_ready)
  {
    otos.calibrateImu();
    otos.resetTracking();
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500); // Give USB serial and HMC6343 time to settle

  Wire.begin();

  otos_ready = otos.begin();
  if (otos_ready)
  {
    otos.setLinearUnit(kSfeOtosLinearUnitMeters);
    otos.setAngularUnit(kSfeOtosAngularUnitRadians);
    Serial.println("STATUS,OTOS_READY");
  }
  else
  {
    Serial.println("STATUS,OTOS_INIT_FAILED");
  }

  compass_ready = compass.init();
  if (compass_ready)
  {
    Serial.println("STATUS,HMC_READY");
  }
  else
  {
    Serial.println("STATUS,HMC_INIT_FAILED");
  }

  resetSensors();
  last_bridge_seen_ms = millis();
}

void handleCommand(const String &command)
{
  if (command == "START")
  {
    streaming_enabled = true;
    last_bridge_seen_ms = millis();
    Serial.println("STATUS,STREAM_ON");
    return;
  }

  if (command == "STOP")
  {
    streaming_enabled = false;
    Serial.println("STATUS,STREAM_OFF");
    return;
  }

  if (command == "PING")
  {
    last_bridge_seen_ms = millis();
    return;
  }

  if (command == "RESET")
  {
    resetSensors();
    last_otos_publish_ms = 0;
    last_compass_publish_ms = 0;
    last_bridge_seen_ms = millis();
    Serial.println("STATUS,RESET_DONE");
    return;
  }

  Serial.print("STATUS,UNKNOWN_COMMAND,");
  Serial.println(command);
}

void pollCommands()
{
  while (Serial.available() > 0)
  {
    const char incoming = (char)Serial.read();
    if (incoming == '\r')
      continue;

    if (incoming == '\n')
    {
      if (command_buffer.length() > 0)
      {
        handleCommand(command_buffer);
        command_buffer = "";
      }
      continue;
    }

    command_buffer += incoming;
  }
}

void publishOtos(unsigned long now_ms)
{
  sfe_otos_pose2d_t pose;
  otos.getPosition(pose);

  Serial.print("OTOS,");
  Serial.print(now_ms);
  Serial.print(",");
  Serial.print(pose.x, 6);
  Serial.print(",");
  Serial.print(pose.y, 6);
  Serial.print(",");
  Serial.println(pose.h, 6);
}

void publishCompass(unsigned long now_ms)
{
  compass.readHeading();
  compass.readAccel();

  const float roll_rad = ((float)compass.roll / 10.0f) * kDegToRad;
  const float pitch_rad = ((float)compass.pitch / 10.0f) * kDegToRad;
  const float heading_rad = ((float)compass.heading / 10.0f) * kDegToRad;
  const float accel_x_mps2 = ((float)compass.accelX / 1024.0f) * kGToMps2;
  const float accel_y_mps2 = ((float)compass.accelY / 1024.0f) * kGToMps2;
  const float accel_z_mps2 = ((float)compass.accelZ / 1024.0f) * kGToMps2;

  Serial.print("HMC,");
  Serial.print(now_ms);
  Serial.print(",");
  Serial.print(roll_rad, 6);
  Serial.print(",");
  Serial.print(pitch_rad, 6);
  Serial.print(",");
  Serial.print(heading_rad, 6);
  Serial.print(",");
  Serial.print(accel_x_mps2, 6);
  Serial.print(",");
  Serial.print(accel_y_mps2, 6);
  Serial.print(",");
  Serial.println(accel_z_mps2, 6);
}

void loop()
{
  pollCommands();

  const unsigned long now_ms = millis();
  bool published = false;

  if (streaming_enabled && (now_ms - last_bridge_seen_ms >= kBridgeTimeoutMs))
  {
    streaming_enabled = false;
    Serial.println("STATUS,BRIDGE_TIMEOUT");
  }

  if (streaming_enabled && otos_ready && (now_ms - last_otos_publish_ms >= kOtosPublishIntervalMs))
  {
    last_otos_publish_ms = now_ms;
    publishOtos(now_ms);
    published = true;
  }

  if (streaming_enabled && compass_ready && (now_ms - last_compass_publish_ms >= kCompassPublishIntervalMs))
  {
    last_compass_publish_ms = now_ms;
    publishCompass(now_ms);
    published = true;
  }

  if (!otos_ready && !compass_ready && (now_ms - last_status_ms >= kStatusIntervalMs))
  {
    last_status_ms = now_ms;
    Serial.println("STATUS,NO_SENSORS_READY");
  }

  if (!published)
    delay(2);
}
