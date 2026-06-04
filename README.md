# IC_Tester

[Video Demostración](https://drive.google.com/file/d/17oGDeb1n3LcxcQWol-RU2FpxxYQ4iN93/view?usp=sharing)

## Descripción

**IC_Tester** es un sistema de instrumentación electrónica diseñado para realizar pruebas automatizadas en circuitos integrados de la serie 74xx. Permite verificar el funcionamiento de compuertas lógicas mediante generación de señales digitales y análisis de respuestas.

El sistema está pensado como un prototipo funcional de bajo costo para validación de ICs.

## Características principales

* Prueba automática de compuertas lógicas (PASS / FAIL)
* Generación de señales digitales con microcontrolador
* Comparación contra tablas de verdad
* Monitoreo de corriente en tiempo real (INA219)
* Detección de ICs dañados (pines defectuosos)
* Interfaz en PC para control y visualización

## Arquitectura del sistema

El sistema se compone de:

* **Arduino Nano**: genera señales de prueba y controla el flujo
* **PCB personalizada**: integra todos los componentes
* **Socket ZIF-14**: inserción rápida de ICs
* **INA219**: medición de corriente vía I2C
* **Interfaz en PC**: comunicación serial (VISA)

### Flujo de operación

1. Inserción del IC
2. Aplicación de entradas digitales
3. Lectura de salidas
4. Comparación con valores esperados
5. Resultado final: PASS / FAIL

## Tecnologías utilizadas

* Arduino
* Visual Studio
* LabVIEW (NI-VISA para comunicación serial)
* Diseño de PCB

## Hardware (BOM resumido)

* Arduino Nano
* INA219 Current Sensor
* LM7805 (regulador)
* Socket ZIF-14
* Capacitores y resistencias
* PCB personalizada

## Resultados

* ICs funcionales → `RESULT: PASS`
* ICs dañados → `RESULT: FAIL`
* El sistema identifica correctamente grupos de compuertas defectuosas

