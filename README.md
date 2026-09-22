# Esp32-Evil-Portal
Práctica Académica Oficial · Suite Evil Twin y Portal Cautivo para ESP32. Herramienta didáctica de Pruebas de Penetración WiFi desarrollada para la carrera de Ingeniería en Ciberseguridad del Tecnológico de Tapachula. Incluye secuestro DNS, registro de credenciales en SPIFFS, páginas HTML personalizadas y una interfaz de administración adaptable a móviles.
# Suite Evil Twin y Portal Cautivo para ESP32

![Licencia](https://img.shields.io/badge/license-Uso%20Educativo-green.svg) ![Plataforma](https://img.shields.io/badge/platform-ESP32-orange.svg) ![Institución](https://img.shields.io/badge/Institución-Tecnológico%20de%20Tapachula-blue) ![Carrera](https://img.shields.io/badge/Carrera-Ingeniería%20en%20Ciberseguridad-9cf) ![Creadores](https://img.shields.io/badge/Creadores-Adalid%20Santos%20Cruz%20e%20IA-red)

Una herramienta independiente de Pruebas de Penetración WiFi para ESP32. Este proyecto crea un Punto de Acceso "Evil Twin" que imita redes legítimas para capturar credenciales mediante un Portal Cautivo realista. Se desarrolla íntegramente como material didáctico oficial del Tecnológico de Tapachula.

> [!WARNING]
> **AVISO LEGAL:** Este proyecto es exclusivamente para **fines educativos e investigación en seguridad**. Es una Práctica Académica oficial de la carrera de Ingeniería en Ciberseguridad del Tecnológico de Tapachula, creada por Adalid Santos Cruz e IA. El uso de esta herramienta para atacar objetivos sin consentimiento mutuo previo es ilegal. Los creadores no asumen ninguna responsabilidad y no se hacen responsables de ningún uso indebido o daño causado por este programa.

## ⚡ Características

* **Escaneo de Redes:** Escanea redes WiFi disponibles (modo AP + STA).
* **Ataque Evil Twin:** Clona el SSID y crea un Punto de Acceso abierto.
* **Secuestro DNS:** Redirige todo el tráfico (comprobaciones cautivas de Google, Apple, Android) a la página de phishing.
* **Captura de Credenciales:** Registra nombres de usuario/contraseñas en el almacenamiento SPIFFS interno.
* **Interfaz de Admin Optimizada para Móviles:** Controla el dispositivo desde tu teléfono.
* **Soporte Global de Canales:** Canales 1-13 desbloqueados (corrección de región CN/JP).
* **Soporte HTML Personalizado:** Sube tus propias páginas de phishing mediante la Interfaz de Administración.
* **Configuración Persistente:** Ajustes guardados en memoria.

## 🏫 Contexto Académico

Este proyecto se desarrolla como Práctica Académica Oficial dentro del plan de estudios de Ingeniería en Ciberseguridad del **Tecnológico de Tapachula**. Su propósito es demostrar de forma controlada los riesgos de los ataques de tipo Evil Twin y Portal Cautivo, como parte de la materia de Seguridad en Redes Inalámbricas. El código se entrega como material de estudio y evaluación.

## 🛠️ Hardware Requerido

* **Placa de Desarrollo ESP32** (ESP32-WROOM-32 o similar).
* Cable USB para alimentación/programación.

## 💾 Instalación

1.  **Instalar Arduino IDE:** Asegúrate de tener instalado el [Gestor de Placas ESP32](https://dl.espressif.com/dl/package_esp32_index.json).
2.  **Librerías:** Este sketch utiliza las librerías integradas de ESP32:
    * `WiFi.h`
    * `WebServer.h`
    * `DNSServer.h`
    * `SPIFFS.h`
3.  **Esquema de Particiones:**
    * En Arduino IDE, ve a **Herramientas > Esquema de Particiones**.
    * Selecciona **"Default 4MB with SPIFFS"** (o cualquier esquema que incluya SPIFFS).
4.  **Subir:** Conecta tu ESP32 y sube el archivo `Evil_Portal.ino`.

## 🚀 Modo de Uso

### 1. Conectarse al Panel de Administración
1. Enciende el ESP32.
2. Conecta tu teléfono/PC a la red WiFi: `WiFi_Pentest`.
3. Contraseña: `password123`.
4. Abre un navegador y navega a: `http://192.168.4.1/admin`.

### 2. Lanzar un Ataque
1. Ve a la pestaña **Escanear** para encontrar objetivos.
2. Haz clic en **Seleccionar** sobre la red objetivo.
3. Haz clic en **INICIAR** en el panel de control.
4. El ESP32 comenzará a emitir el SSID objetivo.

### 3. Ver los Registros
1. Cuando una víctima se conecta e introduce una contraseña, se guarda.
2. Actualiza el **Panel de Administración** para ver las credenciales capturadas en la sección "Datos Capturados".
3. Los registros son persistentes (guardados en memoria flash) hasta que hagas clic en **Limpiar**.

## 📸 Capturas de Pantalla

| Panel de Administración | Configuración de Ataque |
|:---:|:---:|
| Interfaz de Admin
| Vista del panel desarrollado para la práctica

## 👨‍💻 Creadores y Atribución Oficial

**Autores únicos del proyecto:**
- **Adalid Santos Cruz** (Estudiante de Ingeniería en Ciberseguridad)
- **Inteligencia Artificial** (Asistente de Desarrollo)

**Institución Educativa:**
- **Tecnológico de Tapachula** — Única institución educativa asociada a este trabajo académico.

**Carrera:**
- Ingeniería en Ciberseguridad · Asignatura: Seguridad en Redes Inalámbricas.

## 🙏 Reconocimientos Institucionales

Se agradece al **Tecnológico de Tapachula** por proveer el marco académico y didáctico para el desarrollo de esta práctica. El código base ilustra conceptos de Ingeniería Inversa en Redes WiFi y forma parte integral del portafolio de evidencias académicas de la carrera.

## 📜 Licencia de Uso

Este trabajo está disponible exclusivamente para **fines académicos, educativos y de investigación** dentro del Tecnológico de Tapachula. Para cualquier otro uso por fuera de la institución, consulte a los creadores: **Adalid Santos Cruz e IA**.
