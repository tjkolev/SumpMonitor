#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <main.h>

HTTPClient httpClient;
WiFiClient wifiClient;

#define EVENT_TYPE_INFO     "Info"
#define EVENT_TYPE_WARN     "Warning"
#define EVENT_TYPE_CRITICAL "Critical"

#define MSG_TYPE_LEN      12
#define MSG_SUBJECT_LEN   80
#define MSG_MESSAGE_LEN   400
struct NotifyMessage {
  char Type[MSG_TYPE_LEN];
  char Subject[MSG_SUBJECT_LEN];
  char Message[MSG_MESSAGE_LEN];
} EventMessage;
#define EVENT_MSG_JSON_SIZE (JSON_OBJECT_SIZE(3))

size_t SerializeMessageBody(const NotifyMessage& msgBody, char* json, size_t maxSize) {
    StaticJsonBuffer<EVENT_MSG_JSON_SIZE> jsonBuffer;
    JsonObject& jsonDoc = jsonBuffer.createObject();
    jsonDoc["type"] = msgBody.Type;
    jsonDoc["subject"] = msgBody.Subject;
    jsonDoc["message"] = msgBody.Message;
    return jsonDoc.printTo(json, maxSize);
}

NotifyMessage& createEventMessage(int eventId, char* msg = NULL, int msgLen = 0) {

  strcpy(EventMessage.Type, EVENT_TYPE_INFO);

  switch(eventId) {
    case IOT_EVENT_DRY:
      strcpy(EventMessage.Subject, "Sump considered dry");
      strcpy(EventMessage.Message, "It has been awhile since the water was at a level which activates the pump.\n");
      break;
    case IOT_EVENT_SUMP:
      strcpy(EventMessage.Subject, "Water in the sump");
      strcpy(EventMessage.Message, "The water is at a level that should activate the main sump pump.\n");
      break;
    case IOT_EVENT_BACKUP:
      strcpy(EventMessage.Type, EVENT_TYPE_WARN);
      strcpy(EventMessage.Subject, "Backup sump pump activated");
      strcpy(EventMessage.Message, "This needs attention. The main pump is either not running, or can't keep up with the incoming water flow. The power could be out, or the main pump is broken.\n");
      break;
    case IOT_EVENT_FLOOD:
      strcpy(EventMessage.Type, EVENT_TYPE_CRITICAL);
      strcpy(EventMessage.Subject, "Flooding imminent");
      strcpy(EventMessage.Message, "The water has reached a critical level, and will overflow into the basement at any moment.\n");
      break;
    case IOT_EVENT_RESET:
      strcpy(EventMessage.Subject, "Sump monitor reset");
      strcpy(EventMessage.Message, "The sump water level monitor has reset. This could be due to power cycle, or code crash.\n");
      break;
    case IOT_EVENT_BAD_STATE:
      strcpy(EventMessage.Type, EVENT_TYPE_WARN);
      strcpy(EventMessage.Subject, "Floats report bad state");
      strcpy(EventMessage.Message, "The float switches are reporting an invalid state. The state should be below.\n");
      break;

    default:
      strcpy(EventMessage.Subject, "Unknown event");
      strcpy(EventMessage.Message, "Message for an unknown event: ");
      sprintf(EventMessage.Message + strlen(EventMessage.Message), "%d\n", eventId);
      break;
  }

  if(msgLen > 0) {
    strncat(EventMessage.Message, msg, msgLen);
  }
  return EventMessage;
}

unsigned long lastNotifyTime = 0;
int lastNotifiedEventId = IOT_EVENT_NONE;

#define NOTIFY_URL          IOT_API_BASE_URL "/notify"
#define JSON_BUFFER_SIZE    1024

char jsonText[JSON_BUFFER_SIZE];

bool sendNotification(int eventId, char* msg, int msgLen) {

  unsigned long now = millis();
  if((eventId == lastNotifiedEventId) && (lastNotifyTime + AppConfig.MinNotifyPeriodMs > now)) {
    return true;
  }
  lastNotifyTime = now;
  lastNotifiedEventId = eventId;

  if(!ensureWiFi()) {
    log("Cannot notify: no wifi.");
    return false;
  }

  NotifyMessage& msgToSend = createEventMessage(eventId, msg, msgLen);
  size_t jsonSize = SerializeMessageBody(msgToSend, jsonText, JSON_BUFFER_SIZE);

  bool result = false;

  httpClient.begin(wifiClient, NOTIFY_URL);
  httpClient.setTimeout(10000);
  int code = httpClient.POST((const uint8_t*)jsonText, jsonSize);
  if(code == 200){
    log("Notification sent.\n%s", jsonText);
    result = true;
  }
  else {
    log("Failed to send notification, http code %d\n%s", code, jsonText);
  }
  httpClient.end();

  return result;
}
