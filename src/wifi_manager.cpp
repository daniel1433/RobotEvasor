#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "Flia_pardo_alfonso";
const char *password = "P4Rd041F0Nn50";


void iniciarWifi(){
    Serial.println("Iniciando Wifi");
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