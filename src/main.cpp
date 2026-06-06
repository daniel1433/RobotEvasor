#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "main.h"
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
const int LINEA_ACTIVA = HIGH; // Cambia a HIGH si tus sensores devuelven HIGH al detectar la línea

// // Pin del Servo S90
#define PIN_SERVO 5

// // Pines del Sensor Ultrasónico
#define ECHO 17
#define TRIG 16

// Pines Motor A
#define ENA 18
#define IN1 21
#define IN2 19

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
const float COMPENSACION_MOTOR_DERECHO = 1.10; // Reduce ligeramente el motor derecho para compensar deriva hacia la derecha

// Variables globales
float distanciaFrente = 0;
float distanciaDerecha = 0;
float distanciaIzquierda = 0;
bool left = false;
bool right = false;
bool movimiento = true;

// Estado de búsqueda cuando se pierde la línea
unsigned long searchStartMillis = 0;
unsigned long searchPhaseStart = 0;
int searchPhase = 0; // 0 = arco a la derecha, 1 = avance corto
const unsigned long SEARCH_TIMEOUT_MS = 1200; // tiempo máximo buscando antes de intentar avanzar
const unsigned long ARC_DURATION_MS = 150;     // duración del arco a la derecha
const unsigned long FORWARD_DURATION_MS = 200; // duración del avance corto



void setup()
{
  Serial.begin(115200);
  Serial.println("--- Inicializando Robot ESP32 ---");

  // Configuración de pines de motores
  // pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  // pinMode(ENB, OUTPUT);
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

  // cambiarVelocidad(80); // Velocidad inicial al 50%
  // while(true)
  //   {
  //       Serial.println("Prueba adelante");
  //       avanzar();
  //       delay(3000);

  //       Serial.println("Prueba adelante");
  //       retroceder();
  //       delay(3000);

  //   }
}

void loop()
{
  validarConexionMQTT();

  if (MODO_ACTUAL == EVASOR)
    modoEvaSorObstaculos();
  else if (MODO_ACTUAL == SEGUIDOR)
    modoSeguidorLinea();

  int lineaIzq = digitalRead(SENSOR_LINEA_IZQ);

  Serial.print("Sensor Izquierdo: ");
  Serial.println(lineaIzq);

  delay(10);
}
// ============================================================================
// LÓGICA MODO 1: SEGUIDOR DE LÍNEA PURO
// ============================================================================
void modoSeguidorLinea()
{
  int lineaIzq = digitalRead(SENSOR_LINEA_IZQ);
  bool izquierdaDetecta = lineaIzq == LINEA_ACTIVA;

  // DEBUG: muestra el valor del sensor izquierdo
  Serial.print("IZQ="); Serial.println(lineaIzq);

  int velocidadSeguidor = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;
  if (velocidadSeguidor < VELOCIDAD_MINIMA_LINEA)
  {
    velocidadSeguidor = VELOCIDAD_MINIMA_LINEA;
  }

  // Si detecta la línea, avanzar y resetear búsqueda
  if (izquierdaDetecta)
  {
    searchStartMillis = 0;
    searchPhase = 0;
    avanzar(velocidadSeguidor);
    return;
  }

  // Si no detecta, entrar en modo búsqueda no bloqueante
  unsigned long now = millis();
  if (searchStartMillis == 0)
  {
    searchStartMillis = now;
    searchPhaseStart = now;
    searchPhase = 0;
  }

  unsigned long elapsed = now - searchStartMillis;
  if (elapsed > SEARCH_TIMEOUT_MS)
  {
    // Timeout: avanzar un poco hacia adelante y reintentar
    Serial.println("Busqueda timeout: avance corto para reintentar");
    avanzar(VELOCIDAD_MINIMA_LINEA);
    // resetear estado de búsqueda para la próxima iteración
    searchStartMillis = 0;
    searchPhase = 0;
    return;
  }

  // Alternar entre arco a la derecha y avance corto para permitir
  // que el sensor pase por encima de la línea pese a la deriva.
  if (searchPhase == 0)
  {
    if (now - searchPhaseStart < ARC_DURATION_MS)
    {
      // Arco a la derecha: ambos motores hacia adelante, derecho más lento
      aplicarVelocidadMotores(velocidadSeguidor, constrain((int)(velocidadSeguidor * COMPENSACION_MOTOR_DERECHO * 0.6), 0, 255));
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      return;
    }
    else
    {
      searchPhase = 1;
      searchPhaseStart = now;
    }
  }

  if (searchPhase == 1)
  {
    if (now - searchPhaseStart < FORWARD_DURATION_MS)
    {
      // avance corto hacia adelante
      avanzar(VELOCIDAD_MINIMA_LINEA);
      return;
    }
    else
    {
      searchPhase = 0;
      searchPhaseStart = now;
    }
  }
}

// ============================================================================
// LÓGICA MODO 2: EVASOR DE OBSTÁCULOS PURO
// ============================================================================
void modoEvaSorObstaculos()
{
  distanciaFrente = mirarAlFrente();

  if (distanciaFrente > 0 && distanciaFrente <= DISTANCIA_FRENADO)
  {
    detener();
    delay(DELAY_DETENIDO);

    // Evaluar lado izquierdo
    distanciaIzquierda = mirarIzquierda();
    if (distanciaIzquierda > DISTANCIA_FRENADO && !left)
    {
      girarIzquierda();
      delay(DELAY_GIRO);
      detener();
      delay(DELAY_DETENIDO);
      left = false;
      right = false; // Resetear banderas
      return;
    }
    else
    {
      left = true;
    }

    // Evaluar lado derecho
    distanciaDerecha = mirarDerecha();
    if (distanciaDerecha > DISTANCIA_FRENADO && !right)
    {
      girarDerecha();
      delay(DELAY_GIRO);
      detener();
      delay(DELAY_DETENIDO);
      left = false;
      right = false; // Resetear banderas
      return;
    }
    else
    {
      right = true;
    }

    // Si ambos lados están bloqueados, reversa
    retroceder();
    delay(DELAY_REVERSA);
    detener();
    delay(DELAY_DETENIDO);
    left = false;
    right = false;
  }
  else
  {
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

void girarIzquierdaSuave(int speed)
{
  if (speed < 0)
    speed = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;

  Serial.print("Giro suave izquierda...");
  Serial.println(speed);

  if (!movimiento)
    return;

  int base = speed;
  int lenta = max(VELOCIDAD_GIRO_LINEA / 2, base / 3);
  int rapida = base;

  aplicarVelocidadMotores(lenta, rapida);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void girarDerechaSuave(int speed)
{
  if (speed < 0)
    speed = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;

  Serial.print("Giro suave derecha...");
  Serial.println(speed);

  if (!movimiento)
    return;

  int base = speed;
  int lenta = max(VELOCIDAD_GIRO_LINEA / 2, base / 3);
  int rapida = base;

  // Gira suavemente a la derecha con ambos motores hacia adelante
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
  MODO_ACTUAL = MANUAL;
  Serial.println("Modo Manual activado.");
  detener();
}

void modoEvasor()
{
  MODO_ACTUAL = EVASOR;
  Serial.println("Modo Evasor activado.");
}

void modoSeguidor()
{
  MODO_ACTUAL = SEGUIDOR;
  Serial.println("Modo Seguidor activado.");
  modoSeguidorLinea();
}

void avanzar(int speed)
{
  if (speed < 0)
  {
    speed = VELOCIDAD > 0 ? VELOCIDAD : VELOCIDAD_MINIMA_LINEA;
  }
  Serial.print("Avanzar...");
  Serial.println(speed);

  if (!movimiento)
    return;

  int speedA = speed;
  int speedB = constrain((int)(speed * COMPENSACION_MOTOR_DERECHO), 0, 255);
  aplicarVelocidadMotores(speedA, speedB);

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
  digitalWrite(IN3, LOW);  // CORREGIDO: era HIGH
  digitalWrite(IN4, HIGH); // CORREGIDO: era LOW
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