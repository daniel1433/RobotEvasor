#include "wifi_manager.h"
#include "mqtt_manager.h"
#include <Arduino.h>
#include <ESP32Servo.h>

// --- CONFIGURACIÓN DE MODO DE OPERACIÓN ---
enum ModoRobot
{
  EVASOR,
  SEGUIDOR,
  MANUAL,
  DEBUG_SENSOR_IZQ,
  DEBUG_SENSOR_DER
};
ModoRobot MODO_ACTUAL = MANUAL; // ◄--- CAMBIA AQUÍ EL MODO (EVASOR, SEGUIDOR, MANUAL, DEBUG_SENSOR_IZQ o DEBUG_SENSOR_DER)
// ------------------------------------------

// // Instancia del servo
Servo cabeza;

// // Pines de los Seguidores de Línea
#define SENSOR_LINEA_IZQ 14
#define SENSOR_LINEA_DER 27
const int LINEA_ACTIVA = LOW; // Cambia a HIGH si tus sensores devuelven HIGH al detectar la línea

// // Pin del Servo S90
#define PIN_SERVO 5

// // Pines del Sensor Ultrasónico
#define ECHO 17
#define TRIG 16

// Pines Motor A
#define ENA 18
#define IN1 19
#define IN2 21

// Pines Motor B
#define ENB 25
#define IN3 32
#define IN4 33

// Configuración PWM para ESP32
const int PWM_FREQ = 5000;
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;
const int PWM_RESOLUTION = 8;

// Constantes de calibración
int VELOCIDAD = 0;
const float CM_POR_MICROSEGUNDO = 0.034;

const int DISTANCIA_FRENADO = 25;
const int DELAY_DETENIDO = 200;
const int DELAY_GIRO = 350;
const int DELAY_REVERSA = 300;
const int CENTRO = 50;
const int DERECHA = 85;
const int IZQUIERDA = 15;
const int VELOCIDAD_MINIMA_LINEA = 160;
const int VELOCIDAD_GIRO_LINEA = 120;

// Variables globales
float distanciaFrente = 0;
float distanciaDerecha = 0;
float distanciaIzquierda = 0;
bool left = false;
bool right = false;
bool movimiento = true;

// Prototipos de funciones
float mirarAlFrente();
float medirDistancia();
int mirarIzquierda();
int mirarDerecha();
void avanzar();
void retroceder();
void girarDerecha();
void girarIzquierda();
void detener();
void modoSeguidorLinea();
void debugSensorIzquierdo();
void debugSensorDerecho();
void modoEvaSorObstaculos();
void modoManual();
void modoEvasor();
void cambiarVelocidad(int vel);
void aplicarVelocidad(int speed);
void aplicarVelocidadMotores(int speedA, int speedB);
void girarIzquierdaSuave();
void girarDerechaSuave();

void setup()
{
  Serial.begin(115200);
  Serial.println("--- Inicializando Robot ESP32 ---");

  // Configuración de pines de motores
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Configuración de PWM para ESP32
  ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(ENA, PWM_CHANNEL_A);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(ENB, PWM_CHANNEL_B);

  // Configuración de seguidores de línea
  pinMode(SENSOR_LINEA_IZQ, INPUT);
  pinMode(SENSOR_LINEA_DER, INPUT);

  // // Inicialización del servo
  cabeza.attach(PIN_SERVO);
  cabeza.write(CENTRO); // Centrar la cabeza

  // // Pines del ultrasónico
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  iniciarWifi();
  iniciarMQTT();
  delay(1000);

  if (MODO_ACTUAL == EVASOR)
  {
    Serial.println("¡Sistema Listo! MODO: Evasor de Obstáculos Activo.");
  }
  else if (MODO_ACTUAL == SEGUIDOR)
  {
    Serial.println("¡Sistema Listo! MODO: Seguidor de Línea Activo.");
  }
  else
  {
    Serial.println("¡Sistema Listo! MODO: Manual Activo.");
  }
}

void loop()
{
  validarConexionMQTT();

  // Aplicar velocidad constantemente si hay movimiento
  if (movimiento)
  {
    aplicarVelocidad(VELOCIDAD);
  }

  // Ejecutar el modo correspondiente
  if (MODO_ACTUAL == EVASOR)
  {
    modoEvaSorObstaculos();
  }
  else if (MODO_ACTUAL == SEGUIDOR)
  {
    modoSeguidorLinea();
  }
  else if (MODO_ACTUAL == DEBUG_SENSOR_IZQ)
  {
    debugSensorIzquierdo();
  }
  else if (MODO_ACTUAL == DEBUG_SENSOR_DER)
  {
    debugSensorDerecho();
  }
  else
  {
    modoManual();
  }

  delay(10); // Pausa de estabilidad para los motores
}

// ============================================================================
// LÓGICA MODO 1: SEGUIDOR DE LÍNEA PURO
// ============================================================================
void modoSeguidorLinea()
{
  int lineaIzq = digitalRead(SENSOR_LINEA_IZQ);
  int lineaDer = digitalRead(SENSOR_LINEA_DER);

  bool izquierdaDetecta = lineaIzq == LINEA_ACTIVA;
  bool derechaDetecta = lineaDer == LINEA_ACTIVA;

  // DEBUG: descomenta si necesitas ver los valores de los sensores
  // Serial.print("IZQ="); Serial.print(lineaIzq);
  // Serial.print(" DER="); Serial.println(lineaDer);

  // El sensor derecho es el principal para seguir la línea.
  if (derechaDetecta)
  {
    avanzar();
  }
  else if (izquierdaDetecta)
  {
    // El izquierdo solo ayuda a corregir la ruta cuando se pierde el lineamiento derecho.
    girarIzquierdaSuave();
  }
  else
  {
    // Línea perdida: buscar la línea girando poco a la derecha
    // para tratar de volver a colocarla bajo el sensor principal.
    Serial.println("Línea perdida: buscando con el derecho...");
    girarDerechaSuave();
  }
}

// ============================================================================
// FUNCIONES DE DIAGNÓSTICO PARA SENSORES
// ============================================================================
void debugSensorIzquierdo()
{
  int lineaIzq = digitalRead(SENSOR_LINEA_IZQ);
  bool izquierdaDetecta = lineaIzq == LINEA_ACTIVA;

  Serial.print("DEBUG IZQ: valor="); Serial.print(lineaIzq);
  Serial.print(" detecta="); Serial.println(izquierdaDetecta ? "SÍ" : "NO");

  if (izquierdaDetecta)
  {
    avanzar();
  }
  else
  {
    detener();
  }
}

void debugSensorDerecho()
{
  int lineaDer = digitalRead(SENSOR_LINEA_DER);
  bool derechaDetecta = lineaDer == LINEA_ACTIVA;

  Serial.print("DEBUG DER: valor="); Serial.print(lineaDer);
  Serial.print(" detecta="); Serial.println(derechaDetecta ? "SÍ" : "NO");

  if (derechaDetecta)
  {
    avanzar();
  }
  else
  {
    detener();
  }
}

// ============================================================================
// LÓGICA MODO 2: EVASOR DE OBSTÁCULOS PURO
// ============================================================================
void modoEvaSorObstaculos() {
  distanciaFrente = mirarAlFrente();

  if (distanciaFrente > 0 && distanciaFrente <= DISTANCIA_FRENADO) {
    detener();
    delay(DELAY_DETENIDO);

    // Evaluar lado izquierdo
    distanciaIzquierda = mirarIzquierda();
    if (distanciaIzquierda > DISTANCIA_FRENADO && !left) {
      girarIzquierda();
      delay(DELAY_GIRO);
      detener();
      delay(DELAY_DETENIDO);
      left = false; right = false; // Resetear banderas
      return; 
    } else {
      left = true;
    }

    // Evaluar lado derecho
    distanciaDerecha = mirarDerecha();
    if (distanciaDerecha > DISTANCIA_FRENADO && !right) {
      girarDerecha();
      delay(DELAY_GIRO);
      detener();
      delay(DELAY_DETENIDO);
      left = false; right = false; // Resetear banderas
      return;
    } else {
      right = true;
    }

    // Si ambos lados están bloqueados, reversa
    retroceder();
    delay(DELAY_REVERSA);
    detener();
    delay(DELAY_DETENIDO);
    left = false; right = false;
  } 
  else {
    avanzar(); // Si no hay nada al frente, avanza libremente
  }
}


float mirarAlFrente()
{
  cabeza.write(CENTRO);
  return medirDistancia();
}

int mirarIzquierda()
{
  cabeza.write(IZQUIERDA);
  delay(400);
  return medirDistancia();
}

int mirarDerecha()
{
  cabeza.write(DERECHA);
  delay(400);
  return medirDistancia();
}

float medirDistancia()
{
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duracion = pulseIn(ECHO, HIGH, 25000);
  float distancia = duracion * CM_POR_MICROSEGUNDO / 2;

  Serial.print("Distancia: ");
  Serial.println(distancia);
  return distancia;
}

void aplicarVelocidad(int speed)
{
  speed = constrain(speed, 0, 255);

  ledcWrite(PWM_CHANNEL_A, speed);
  ledcWrite(PWM_CHANNEL_B, speed);
}

void aplicarVelocidadMotores(int speedA, int speedB)
{
  speedA = constrain(speedA, 0, 255);
  speedB = constrain(speedB, 0, 255);

  ledcWrite(PWM_CHANNEL_A, speedA);
  ledcWrite(PWM_CHANNEL_B, speedB);
}

void girarIzquierdaSuave()
{
  Serial.print("Giro suave izquierda...");
  Serial.println(VELOCIDAD);

  if (!movimiento)
    return;

  int base = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;
  int lenta = max(VELOCIDAD_GIRO_LINEA / 2, base / 3);
  int rapida = base;

  aplicarVelocidadMotores(lenta, rapida);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void girarDerechaSuave()
{
  Serial.print("Giro suave derecha...");
  Serial.println(VELOCIDAD);

  if (!movimiento)
    return;

  int base = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;
  int lenta = max(VELOCIDAD_GIRO_LINEA / 2, base / 3);
  int rapida = base;

  aplicarVelocidadMotores(rapida, lenta);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void cambiarVelocidad(int vel)
{
  vel = constrain(vel, 0, 100);

  if (vel == 0)
  {
    VELOCIDAD = 0;
  }
  else
  {
    VELOCIDAD = map(vel, 1, 100, 160, 255);
  }
  Serial.print("Velocidad ajustada: ");
  Serial.println(VELOCIDAD);
}

void modoManual()
{
  // En modo manual, no hace nada - espera órdenes por MQTT
  // Las órdenes de movimiento vienen vía: avanzar(), retroceder(), girarDerecha(), girarIzquierda(), detener()
}

void modoEvasor()
{
  MODO_ACTUAL = EVASOR;
}

void modoSeguidor()
{
  MODO_ACTUAL = SEGUIDOR;
  modoSeguidorLinea();
}

void avanzar()
{
  Serial.print("Avanzar...");
  Serial.println(VELOCIDAD);

  if (!movimiento)
    return;

  aplicarVelocidad(VELOCIDAD);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void retroceder()
{
  Serial.print("Reversa...");
  Serial.println(VELOCIDAD);
  if (!movimiento)
    return;

  aplicarVelocidad(VELOCIDAD);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarDerecha()
{
  Serial.print("Derecha...");
  Serial.println(VELOCIDAD);
  if (!movimiento)
    return;
  aplicarVelocidad(VELOCIDAD);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarIzquierda()
{
  Serial.print("Izquierda...");
  Serial.println(VELOCIDAD);
  if (!movimiento)
    return;
  aplicarVelocidad(VELOCIDAD);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void detener()
{
  aplicarVelocidad(0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}