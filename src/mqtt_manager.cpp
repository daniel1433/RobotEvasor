#include "mqtt_manager.h"
#include "wifi_manager.h"
#include "main.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char *MQTT_SERVER = "192.168.1.11"; // IP de tu PC
const int MQTT_PORT = 1883;
const char *MQTT_USER = "jdpardo";
const char *MQTT_PASSWORD = "1234567a";
const char *TOPIC_SENSORES_ADELANTE = "ROBOT/ADELANTE";
const char *TOPIC_SENSORES_ATRAS = "ROBOT/ATRAS";
const char *TOPIC_SENSORES_DERECHA = "ROBOT/DERECHA";
const char *TOPIC_SENSORES_IZQUIERDA = "ROBOT/IZQUIERDA";
const char *TOPIC_SENSORES_DETENER = "ROBOT/DETENER";
const char *TOPIC_SENSORES_MODO = "ROBOT/MODO";
const char *TOPIC_SENSORES_VELOCIDAD = "ROBOT/VELOCIDAD";
const char *TOPIC_SERVOMOTOR = "ROBOT/SERVOMOTOR";


WiFiClient espClient;
PubSubClient mqttClient(espClient);


// =========================
// CONEXION MQTT
// =========================

void iniciarMQTT()
{
  Serial.println("Iniciando variables MQTT...");
  delay(1000);
  mqttClient.setCallback(callback);
}

void loopMQTT()
{
}

void conectarMQTT()
{
  static unsigned long ultimoIntento = 0;

  if (mqttClient.connected())
  {
    return;
  }

  if (millis() - ultimoIntento < 2000)
  {
    return;
  }

  ultimoIntento = millis();

  Serial.print("Conectando MQTT...");

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  bool conectado = mqttClient.connect(
      "ESP32_CLIENT",
      MQTT_USER,
      MQTT_PASSWORD);

  if (conectado)
  {
    Serial.println("OK");

    mqttClient.subscribe(TOPIC_SENSORES_ADELANTE);
    mqttClient.subscribe(TOPIC_SENSORES_ATRAS);
    mqttClient.subscribe(TOPIC_SENSORES_DERECHA);
    mqttClient.subscribe(TOPIC_SENSORES_IZQUIERDA);
    mqttClient.subscribe(TOPIC_SENSORES_DETENER);
  
    mqttClient.subscribe(TOPIC_SENSORES_MODO);
    mqttClient.subscribe(TOPIC_SENSORES_VELOCIDAD);
    mqttClient.subscribe(TOPIC_SERVOMOTOR);
  }
  else
  {
    Serial.print("ERROR rc=");
    Serial.println(mqttClient.state());
  }
}

bool validarConexionMQTT()
{
  if (!validarConexionWiFi())
  {
    Serial.println("Sin conexión wifi");
    return false;
  }

  conectarMQTT();

  mqttClient.loop();

  return mqttClient.connected();
}

// =========================
// ENVIAR MENSAJE TEXTO
// =========================

void enviarMensaje(const char *topic, String mensaje)
{
  bool connection = validarConexionMQTT();
  if (connection)
  {
    mqttClient.publish(topic, mensaje.c_str());

    Serial.println("Mensaje enviado:");
    Serial.println(mensaje);
  }
  else
  {
    Serial.println("Error al enviar mensaje MQTT");
  }
}


void callback(char *topic, byte *payload, unsigned int length)
{

  String mensaje = "";

  for (int i = 0; i < length; i++)
  {
    mensaje += (char)payload[i];
  }

  Serial.println("********************");
  Serial.print("Mensaje recibido: ");
  Serial.println(mensaje);
  Serial.println("********************");

  if (String(topic) == TOPIC_SENSORES_ADELANTE)
  {
    avanzar();
    return;
  }
  if (String(topic) == TOPIC_SENSORES_ATRAS)
  {
    retroceder();
    return;
  }

  if (String(topic) == TOPIC_SENSORES_DERECHA)
  {
    girarDerecha();
    return;
  }

  if (String(topic) == TOPIC_SENSORES_IZQUIERDA)
  {
    girarIzquierda();
    return;
  }

  if (String(topic) == TOPIC_SENSORES_DETENER)
  {
    detener();
    return;
  }
  
  if (String(topic) == TOPIC_SENSORES_MODO)
  {
    if(mensaje == "EVASOR"){
      modoEvasor();
      return;
    }
    if(mensaje == "SEGUIDOR"){
      modoSeguidor();
      return;
    }
    if(mensaje == "MANUAL"){
      modoManual();
      return;
    }

    if(mensaje == "DEBUG_SENSOR_IZQ"){
      debugSensorIzquierdo();
      return;
    }

    if(mensaje == "DEBUG_SENSOR_DER"){
      debugSensorDerecho();
      return;
    }
  }

  if (String(topic) == TOPIC_SERVOMOTOR)
  {
    if(mensaje == "FRENTE"){
      mirarAlFrente();
      return;
    }
    if(mensaje == "DERECHA"){
      mirarDerecha();
      return;
    }
    if(mensaje == "IZQUIERDA"){
      mirarIzquierda();
      return;
    }
  }


  if (String(topic) == TOPIC_SENSORES_VELOCIDAD)
  {
    int velocidadRecibida = mensaje.toInt();
    Serial.print("Velocidad MQTT recibida: '");
    Serial.print(mensaje);
    Serial.print("' -> ");
    Serial.println(velocidadRecibida);
    cambiarVelocidad(velocidadRecibida);
    return;
  }
}
