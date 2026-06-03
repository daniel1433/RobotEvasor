#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
// =========================
// MQTT
// =========================
extern const char *MQTT_SERVER;
extern const int MQTT_PORT;

extern const char *MQTT_USER;
extern const char *MQTT_PASSWORD;



// =========================
// TOPICS MQTT
// =========================

extern const char* TOPIC_SENSORES;
extern const char* TOPIC_SEGURIDAD;
extern const char* TOPIC_ESTADO_SEGURIDAD;
extern const char* TOPIC_ESTADO_SEGURIDAD_APP;
extern const char* TOPIC_ESTADO_SEGURIDAD_BTN;
extern const char* TOPIC_ESTADO_SEGURIDAD_GEMELO;
extern const char* TOPIC_SEGURIDAD_TEMP;
extern const char* TOPIC_SEGURIDAD_AGUA;

// =========================
// FUNCTIOS
// =========================
void conectarMQTT();
void enviarMensaje(const char *topic, String mensaje);
bool validarConexionMQTT();
void iniciarMQTT();
void callback(char* topic, byte* payload, unsigned int length);
#endif