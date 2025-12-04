#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>

// ---------- Pins ----------
const uint8_t RELAY_PINS[4] = {32, 33, 25, 26};
const uint8_t LED_PIN = 23;

// ---------- Preferences keys ----------
const char* PREF_NAMESPACE = "relaycfg";

// WiFi
const char* KEY_WIFI_SSID = "ssid";
const char* KEY_WIFI_PASS = "wpass";
const char* KEY_AP_PASS   = "appass"; // SoftAP passphrase

// MQTT
const char* KEY_MQTT_HOST = "mhost";
const char* KEY_MQTT_PORT = "mport";
const char* KEY_MQTT_USER = "muser";
const char* KEY_MQTT_PASS = "mpass";
const char* KEY_MQTT_TLS  = "mtls";
const char* KEY_MQTT_VERIFY = "mverify";
const char* KEY_MQTT_CA = "mca";

// MQTT topics
const char* KEY_MQTT_TOPIC0 = "mtopic0";
const char* KEY_MQTT_TOPIC1 = "mtopic1";
const char* KEY_MQTT_TOPIC2 = "mtopic2";
const char* KEY_MQTT_TOPIC3 = "mtopic3";

// ---------- Globals ----------
Preferences prefs;
WebServer webServer(80);

WiFiClient plainClient;
WiFiClientSecure secureClient;
PubSubClient mqttClient;

String wifi_ssid = "";
String wifi_password = "";
String ap_pass = "12345678"; // default SoftAP passphrase
String mqtt_host = "";
uint16_t mqtt_port = 1883;
String mqtt_user = "";
String mqtt_password = "";
bool mqtt_use_tls = false;
bool mqtt_verify_cert = false;
String mqtt_ca_pem = "";

String mqttTopics[4]; // configurable topics

bool configMode = false;
const unsigned long wifiConnectTimeoutMs = 15000;

// ---------- Helpers ----------
String getMacAddress() { return WiFi.macAddress(); }

void setRelay(uint8_t idx, bool on) {
  if(idx>=4) return;
  digitalWrite(RELAY_PINS[idx], on?HIGH:LOW);
}

bool payloadIsOn(const char* payload, unsigned int length) {
  if(length>=2) return (payload[0]=='O'||payload[0]=='o') && (payload[1]=='N'||payload[1]=='n');
  return false;
}

// ---------- Load/Save config ----------
void loadConfig(){
  prefs.begin(PREF_NAMESPACE, true);
  wifi_ssid = prefs.getString(KEY_WIFI_SSID,"");
  wifi_password = prefs.getString(KEY_WIFI_PASS,"");
  ap_pass = prefs.getString(KEY_AP_PASS,"12345678");

  mqtt_host = prefs.getString(KEY_MQTT_HOST,"");
  mqtt_port = prefs.getUInt(KEY_MQTT_PORT,1883);
  mqtt_user = prefs.getString(KEY_MQTT_USER,"");
  mqtt_password = prefs.getString(KEY_MQTT_PASS,"");
  mqtt_use_tls = prefs.getBool(KEY_MQTT_TLS,false);
  mqtt_verify_cert = prefs.getBool(KEY_MQTT_VERIFY,false);
  mqtt_ca_pem = prefs.getString(KEY_MQTT_CA,"");

  // MQTT topics
  mqttTopics[0] = prefs.getString(KEY_MQTT_TOPIC0,"/home/GW/relayboard1/relay1");
  mqttTopics[1] = prefs.getString(KEY_MQTT_TOPIC1,"/home/GW/relayboard1/relay2");
  mqttTopics[2] = prefs.getString(KEY_MQTT_TOPIC2,"/home/GW/relayboard1/relay3");
  mqttTopics[3] = prefs.getString(KEY_MQTT_TOPIC3,"/home/GW/relayboard1/relay4");
  prefs.end();
}

void saveConfig(){
  prefs.begin(PREF_NAMESPACE,false);
  prefs.putString(KEY_WIFI_SSID,wifi_ssid);
  prefs.putString(KEY_WIFI_PASS,wifi_password);
  prefs.putString(KEY_AP_PASS,ap_pass);

  prefs.putString(KEY_MQTT_HOST,mqtt_host);
  prefs.putUInt(KEY_MQTT_PORT,mqtt_port);
  prefs.putString(KEY_MQTT_USER,mqtt_user);
  prefs.putString(KEY_MQTT_PASS,mqtt_password);
  prefs.putBool(KEY_MQTT_TLS,mqtt_use_tls);
  prefs.putBool(KEY_MQTT_VERIFY,mqtt_verify_cert);
  prefs.putString(KEY_MQTT_CA,mqtt_ca_pem);

  prefs.putString(KEY_MQTT_TOPIC0,mqttTopics[0]);
  prefs.putString(KEY_MQTT_TOPIC1,mqttTopics[1]);
  prefs.putString(KEY_MQTT_TOPIC2,mqttTopics[2]);
  prefs.putString(KEY_MQTT_TOPIC3,mqttTopics[3]);
  prefs.end();
}

// ---------- MQTT ----------
void mqttCallback(char* topic, byte* payload, unsigned int length){
  String t = String(topic);
  for(int i=0;i<4;i++){
    if(t==mqttTopics[i]){
      bool on = payloadIsOn((const char*)payload,length);
      setRelay(i,on);
      Serial.printf("MQTT -> %s : %s\n", mqttTopics[i].c_str(), on?"ON":"OFF");
      break;
    }
  }
}

void configureMqttClient(){
  if(mqtt_use_tls){
    Serial.println("MQTT: TLS mode");
    if(mqtt_verify_cert){
      if(mqtt_ca_pem.length()>0) secureClient.setCACert(mqtt_ca_pem.c_str());
      else {
        Serial.println("No CA for verification, using insecure TLS");
        secureClient.setInsecure();
      }
    } else secureClient.setInsecure();
    mqttClient.setClient(secureClient);
  } else mqttClient.setClient(plainClient);
  mqttClient.setServer(mqtt_host.c_str(),mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void mqttReconnectIfNeeded(){
  if(!mqttClient.connected()){
    if(mqtt_host.length()==0) return;
    String clientId="ESP32-relay-"+String((uint32_t)ESP.getEfuseMac(),HEX);
    bool ok;
    if(mqtt_user.length()>0) ok=mqttClient.connect(clientId.c_str(),mqtt_user.c_str(),mqtt_password.c_str());
    else ok=mqttClient.connect(clientId.c_str());
    if(ok){
      Serial.println("MQTT connected");
      for(int i=0;i<4;i++){
        mqttClient.subscribe(mqttTopics[i].c_str());
        Serial.printf("Subscribed to %s\n", mqttTopics[i].c_str());
      }
    } else {
      Serial.print("MQTT failed rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// ---------- Web UI ----------
WebServer web(80);
String pageHeader(const String& title){
  return "<!doctype html><html><head><meta charset='utf-8'><title>"+title+
         "</title><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<style>body{font-family:Arial;margin:12px;}label{display:block;margin:6px 0 2px;}input,textarea,button{padding:8px;width:100%;box-sizing:border-box;} .row{display:flex;gap:8px;} .col{flex:1;}</style></head><body>";
}
String pageFooter(){ return "</body></html>"; }

String getRelayStatusHtml(){
  String s="<h3>Relays</h3><div>";
  for(int i=0;i<4;i++){
    bool state=digitalRead(RELAY_PINS[i])==HIGH;
    s+="Relay "+String(i+1)+": "+(state?"ON":"OFF")+"<br>";
  }
  s+="</div>";
  return s;
}

void handleRoot(){
  String html = pageHeader("Relay Board");
  html += "<h2>Relay Board</h2>";
  html += "<p>Device MAC: <strong>"+getMacAddress()+"</strong></p>";
  if(configMode) html += "<p><strong>CONFIG MODE</strong></p>";
  else html += "<p>Connected to WiFi: <strong>"+WiFi.SSID()+"</strong></p>";

  // config form
  html += "<form method='POST' action='/save'>";
  html += "<h3>WiFi</h3><label>SSID</label><input name='ssid' value='"+wifi_ssid+"'>";
  html += "<label>Password</label><input name='wpass' value='"+wifi_password+"'>";
  html += "<label>SoftAP Passphrase</label><input name='appass' value='"+ap_pass+"'>";
  
  html += "<h3>MQTT</h3>";
  html += "<label>Host</label><input name='mhost' value='"+mqtt_host+"'>";
  html += "<label>Port</label><input name='mport' value='"+String(mqtt_port)+"'>";
  html += "<label>User</label><input name='muser' value='"+mqtt_user+"'>";
  html += "<label>Password</label><input name='mpass' value='"+mqtt_password+"'>";
  html += "<label><input type='checkbox' name='mtls' "+String(mqtt_use_tls?"checked":"")+"> Use TLS</label>";
  html += "<label><input type='checkbox' name='mverify' "+String(mqtt_verify_cert?"checked":"")+"> Verify certificate</label>";
  html += "<label>Root CA PEM</label><textarea name='mca' rows='6'>"+mqtt_ca_pem+"</textarea>";

  html += "<h3>MQTT Topics</h3>";
  for(int i=0;i<4;i++){
    html += "<label>Relay "+String(i+1)+" topic</label>";
    html += "<input name='topic"+String(i+1)+"' value='"+mqttTopics[i]+"'>";
  }
  html += "<br><button type='submit'>Save & Restart</button></form>";

  html += "<hr>"+getRelayStatusHtml();
  html += pageFooter();
  webServer.send(200,"text/html",html);
}

void handleSave(){
  if(webServer.hasArg("ssid")) wifi_ssid=webServer.arg("ssid");
  if(webServer.hasArg("wpass")) wifi_password=webServer.arg("wpass");
  if(webServer.hasArg("appass")){
    String p=webServer.arg("appass");
    if(p.length()>=8 && p.length()<=63) ap_pass=p;
  }
  if(webServer.hasArg("mhost")) mqtt_host=webServer.arg("mhost");
  if(webServer.hasArg("mport")) mqtt_port=webServer.arg("mport").toInt();
  if(webServer.hasArg("muser")) mqtt_user=webServer.arg("muser");
  if(webServer.hasArg("mpass")) mqtt_password=webServer.arg("mpass");
  mqtt_use_tls = webServer.hasArg("mtls");
  mqtt_verify_cert = webServer.hasArg("mverify");
  if(webServer.hasArg("mca")) mqtt_ca_pem=webServer.arg("mca");

  for(int i=0;i<4;i++){
    String arg = "topic"+String(i+1);
    if(webServer.hasArg(arg)){
      String t = webServer.arg(arg);
      if(t.length()>0) mqttTopics[i]=t;
    }
  }

  saveConfig();
  webServer.send(200,"text/html","<p>Saved. Restarting...</p>");
  delay(500);
  ESP.restart();
}

// ---------- WiFi ----------
bool tryConnectWiFi(){
  if(wifi_ssid.length()==0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(),wifi_password.c_str());
  unsigned long start=millis();
  while(millis()-start<wifiConnectTimeoutMs){
    if(WiFi.status()==WL_CONNECTED){
      Serial.printf("Connected, IP: %s\n",WiFi.localIP().toString().c_str());
      return true;
    }
    delay(200);
  }
  Serial.println("WiFi connect timeout");
  return false;
}

void startConfigPortal(){
  configMode=true;
  digitalWrite(LED_PIN,LOW);
  String apName="RelayCfg-"+getMacAddress();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(),ap_pass.c_str());
  Serial.printf("AP '%s' started with password '%s'\n",apName.c_str(),ap_pass.c_str());

  webServer.on("/",HTTP_GET,handleRoot);
  webServer.on("/save",HTTP_POST,handleSave);
  webServer.begin();
}

// ---------- Setup & Loop ----------
void setup(){
  Serial.begin(115200);
  delay(50);
  pinMode(LED_PIN,OUTPUT); digitalWrite(LED_PIN,LOW);
  for(int i=0;i<4;i++){ pinMode(RELAY_PINS[i],OUTPUT); digitalWrite(RELAY_PINS[i],LOW); }

  loadConfig();
  bool wifiConnected = tryConnectWiFi();

  if(wifiConnected){
    configureMqttClient();
    webServer.on("/",HTTP_GET,handleRoot);
    webServer.on("/save",HTTP_POST,handleSave);
    webServer.begin();
    digitalWrite(LED_PIN,HIGH);
  } else startConfigPortal();
}

void loop(){
  webServer.handleClient();
  if(!configMode){
    if(WiFi.status()!=WL_CONNECTED){
      digitalWrite(LED_PIN,LOW);
      WiFi.disconnect();
      bool re=tryConnectWiFi();
      if(!re){
        webServer.stop();
        startConfigPortal();
      } else {
        webServer.stop();
        webServer.on("/",HTTP_GET,handleRoot);
        webServer.on("/save",HTTP_POST,handleSave);
        webServer.begin();
        digitalWrite(LED_PIN,HIGH);
      }
    } else {
      digitalWrite(LED_PIN,HIGH);
      mqttReconnectIfNeeded();
      if(mqttClient.connected()) mqttClient.loop();
    }
  }
}
