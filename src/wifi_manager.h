#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// =========================
// WIFI
// =========================
extern const char *ssid;
extern const char *password;


// =========================
// ESTRUCTURAS
// =========================
struct ResultadoWifi
{
    String titulo;
    String mensaje;
    String ip;
    bool conectado;
};

// =========================
// FUNCIONES
// =========================

bool validarConexionWiFi();
ResultadoWifi mostrarDatosConexionWiFi();
void iniciarWifi();

#endif