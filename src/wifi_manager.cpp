#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "LaptopJesus";
const char *password = "1234567Aa";


void iniciarWifi(){
    Serial.println("Iniciando Wifi");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   // Evita microcortes por ahorro de energía
    WiFi.begin(ssid, password);

    validarConexionWiFi();
}

bool validarConexionWiFi()
{
  // Serial.println("Conectando.....");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Serial.println("\nConectado!");
  // Serial.print("IP: ");
  // Serial.println(WiFi.localIP());
  return true;
}

ResultadoWifi mostrarDatosConexionWiFi()
{
  ResultadoWifi resultado;

  resultado.titulo = "Conexion: ";
  
  if (WiFi.status() == WL_CONNECTED)
  {
    resultado.titulo += "OK";
  }
  else
  {
    resultado.mensaje += "FALLA";
  }

  resultado.conectado = WiFi.status();
  resultado.ip = WiFi.localIP().toString();

  resultado.mensaje = "IP: " + resultado.ip;

  return resultado;
}