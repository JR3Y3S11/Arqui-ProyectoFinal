# Arduino Security Project

## Proyecto Final — Arquitectura del Computador

Este repositorio contiene el proyecto base para el desarrollo del **Proyecto Final del curso de Arquitectura del Computador**.

El objetivo es construir progresivamente un **Mini Sistema Computacional de Control y Seguridad utilizando Arduino Uno**, integrando los conceptos estudiados durante el curso: entrada y salida de datos, memoria, procesamiento, buses, periféricos, sensores y actuadores.

El proyecto será desarrollado de forma incremental durante los laboratorios.

## Objetivo del proyecto

Construir un sistema de control de acceso capaz de identificar a un usuario, solicitar autenticación y controlar físicamente el acceso mediante diferentes periféricos conectados al Arduino.

El sistema final integrará componentes como:

* Arduino Uno
* RFID RC522
* Teclado matricial 4×4
* Pantalla LCD 16×2
* Servo motor
* LED y buzzer
* Registro de desplazamiento 74HC595

Durante el desarrollo podrán incorporarse otros sensores o dispositivos dependiendo de los requerimientos de cada etapa.

## Estructura del repositorio

```text
arduino-security-project/
│
├── base/
│   └── base.ino
│
├── diagnostics/
│   └── diagnostics.ino
│
├── 3d-printing/
│   └── archivos STL
│
├── pinout.pdf
│
└── README.md
```

### `base/`

Contiene el programa base del proyecto.

Este será el punto de partida para integrar progresivamente las funcionalidades desarrolladas durante los laboratorios.

### `diagnostics/`

Contiene código destinado a realizar pruebas y diagnósticos de los componentes del sistema.

Puede utilizarse para comprobar conexiones y funcionamiento de los dispositivos antes de integrarlos al programa principal.

### `3d-printing/`

Contiene los modelos STL utilizados para construir la estructura física del proyecto.

Incluye piezas para la carcasa, puerta, soportes y mecanismo asociado al servo.

### `pinout.pdf`

Diagrama de referencia para las conexiones utilizadas en el proyecto.
