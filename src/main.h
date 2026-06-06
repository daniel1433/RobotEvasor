#ifndef MAIN_H
#define MAIN_H
// Prototipos de funciones
float mirarAlFrente();
float medirDistancia();
int mirarIzquierda();
int mirarDerecha();
void avanzar(int speed = -1);
void retroceder();
void girarDerecha();
void girarIzquierda();
void detener();
void modoSeguidorLinea();
void modoSeguidor();
void modoEvaSorObstaculos();
void modoManual();
void modoEvasor();
void cambiarVelocidad(int vel);
void aplicarVelocidad(int speed);
void aplicarVelocidadMotores(int speedA, int speedB);
void girarIzquierdaSuave(int speed = -1);
void girarDerechaSuave(int speed = -1);

#endif
