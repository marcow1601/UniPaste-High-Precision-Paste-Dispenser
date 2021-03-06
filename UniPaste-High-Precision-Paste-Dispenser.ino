#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#include "FS.h"
#include <LITTLEFS.h>             // https://github.com/lorol/LITTLEFS

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <TMCStepper.h>
#include <AccelStepper.h>

//#include <Arduino.h>
//#include <ESP32CAN.h>
//#include <CAN_config.h>

#include <CAN.h>

String multipacketCAN;
bool multipacketCANisComplete = false;
unsigned long previousMillis = 0;   // will store last time a CAN Message was send

 

//CAN_device_t CAN_cfg;               // CAN Config
//unsigned long previousMillis = 0;   // will store last time a CAN Message was send
//const int interval = 1000;          // interval at which send CAN Messages (milliseconds)
//const int rx_queue_size = 10;       // Receive Queue size


/***
 * Pin definitions
 ***/
#define LED_0         12
#define LED_1         13
#define LED_2         14

#define TMC_EN        21
#define TMC_MS1       25
#define TMC_MS2       26
#define TMC_DIAG      27
#define TMC_STEP      22
#define TMC_DIR       23
#define TMC_PDN       16

#define RXD2          16
#define TXD2          17

#define CAN_R         4
#define CAN_D         5


/***
 *    Configuration parameters
 ***/

#define USE_UART        false
#define MICROSTEPS      8       // TMC2209 options: 8,16,32,64
#define DRIVER_ADDRESS  0b00 // TMC2209 Driver address according to MS1 and MS2
#define R_SENSE         0.11f

TMC2209Stepper driver(&Serial2, R_SENSE, DRIVER_ADDRESS);
AccelStepper stepper = AccelStepper(stepper.DRIVER, TMC_STEP, TMC_DIR);

/***
 *    Motion system variables
 ***/ 
#define AXIS          'A'

float stepsPerMM = 80;        // This is full steps per mm. Real steps/mm is MICROSTEPS*stepsPerMM

String positioning = "absolute";
float position = 0;
float targetPosition = 0;


// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _WIFIMGR_LOGLEVEL_    3

FS* filesystem =      &LITTLEFS;
#define FileFS        LITTLEFS
#define FS_Name       "LittleFS"

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

// SSID and PW for Config Portal
String ssid = "UniPaste";
const char* password = "unipaste";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// You only need to format the filesystem once
//#define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM         false

#define MIN_AP_PASSWORD_SIZE    8

#define SSID_MAX_LEN            32
//From v1.0.10, WPA2 passwords can be up to 63 characters long.
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESP_WiFiManager.h>
#define USE_AVAILABLE_PAGES     false

#define USE_ESP_WIFIMANAGER_NTP     true
#define USE_CLOUDFLARE_NTP          false
#define USING_CORS_FEATURE          true

#define USE_DHCP_IP     true

IPAddress stationIP   = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP   = IPAddress(192, 168, 178, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);

#define USE_CONFIGURABLE_DNS      true

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

uint8_t connectMultiWiFi(void);

void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

void check_WiFi(void)
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
    Serial.println("\nWiFi lost. Call connectMultiWiFi in loop");
    connectMultiWiFi();
  }
}  

void check_status(void)
{
  static ulong checkstatus_timeout  = 0;
  static ulong checkwifi_timeout    = 0;

  static ulong current_millis;

#define WIFICHECK_INTERVAL    1000L
#define HEARTBEAT_INTERVAL    10000L

  current_millis = millis();
  
  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((current_millis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = current_millis + HEARTBEAT_INTERVAL;
  }
}

void loadConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  if (file)
  {
    file.readBytes((char *) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}
    
void saveConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

uint8_t connectMultiWiFi(void)
{
#if ESP32
  // For ESP32, this better be 0 to shorten the connect time
  #define WIFI_MULTI_1ST_CONNECT_WAITING_MS       0
#else
  // For ESP8266, this better be 2200 to enable connect the 1st time
  #define WIFI_MULTI_1ST_CONNECT_WAITING_MS       2200L
#endif

#define WIFI_MULTI_CONNECT_WAITING_MS           100L
  
  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));
  
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }
  
  LOGERROR(F("Connecting MultiWifi..."));

  WiFi.mode(WIFI_STA);

#if !USE_DHCP_IP    
  #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    WiFi.config(stationIP, gatewayIP, netMask, dns1IP, dns2IP);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    WiFi.config(stationIP, gatewayIP, netMask);
  #endif 
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 10 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

void receiveMultipacketCAN(int packetSize){
  byte multipacketSize, packetIndex;
  
  if(packetSize > 2){
    if(CAN.available()) multipacketSize = CAN.read();
    if(CAN.available()) packetIndex = CAN.read();

    Serial.print(packetIndex);
    Serial.print(" / ");
    Serial.println(multipacketSize);

    if(packetIndex == 1) multipacketCAN = "";
    
    if(packetIndex == multipacketSize) multipacketCANisComplete = true;
    else multipacketCANisComplete = false;
    
    while(CAN.available()) multipacketCAN += (char)CAN.read();
    
  }

  else {
    while(CAN.available()) CAN.read();
    Serial.println("Invalid segment of Multipacket.");
  }
}

void sendMultipacketCAN(String message) {
  Serial.print("Sending Multipacket: ");
  Serial.println(message);

  Serial.print("No. of packets: ");
  uint8_t noPackets = (message.length()%6 > 0) ? (int)(message.length()/6.0)+1 : (int)(message.length()/6.0);
  Serial.println(noPackets);

  for(uint8_t packet = 1; packet <= noPackets; packet++){
    CAN.beginPacket(0x1);
    CAN.write(noPackets); // How many packets for entire multipacket
    CAN.write(packet); // This packet index

    Serial.print(packet);
    Serial.print(" / ");
    Serial.println(noPackets);
    
    if(packet < noPackets || message.length()%6 == 0){  // Not the last packet in multipacket
      for(uint8_t i=0; i<6; i++){
        CAN.write(message.charAt(6*(packet-1)+i));
        Serial.print(message.charAt(6*(packet-1)+i));
      }
    }
    else{  // Last packet in multipacket might be shorter than 6 bytes
      for(uint8_t i=0; i<message.length()%6; i++){
        CAN.write(message.charAt(6*(packet-1)+i));
        Serial.print(message.charAt(6*(packet-1)+i));
      }
    }
    CAN.endPacket();
    Serial.println();
  }
}

void onReceive(int packetSize) {
  // received a packet
  Serial.print("Received ");

  if (CAN.packetExtended()) {
    Serial.print("extended ");
  }

  if (CAN.packetRtr()) {
    // Remote transmission request, packet contains no data
    Serial.print("RTR ");
  }

  Serial.print("packet with id 0x");
  Serial.print(CAN.packetId(), HEX);

  if (CAN.packetRtr()) {
    Serial.print(" and requested length ");
    Serial.println(CAN.packetDlc());
  } else {
    Serial.print(" and length ");
    Serial.println(packetSize);

    receiveMultipacketCAN(packetSize);

    if(multipacketCANisComplete) Serial.println(multipacketCAN);
  }

  Serial.println();
}

void processGCode(String gcode){
  String command = "";
  String arg1 = "";
  String arg2 = "";
  
  if(gcode.length() > 0 && gcode.indexOf(' ') == -1){
    command = gcode;
  }
  else if(gcode.length() > 0 && gcode.indexOf(' ') > -1){
    command = gcode.substring(0,gcode.indexOf(' '));
    String arguments = gcode.substring(gcode.indexOf(' ')+1);

    if(arguments.indexOf(' ') > -1){
      arg1 = arguments.substring(0,arguments.indexOf(' '));
      arg2 = arguments.substring(arguments.indexOf(' ')+1);
    }
    else arg1 = arguments;
  }
  else{
    Serial.println("Empty GCode received");
    return;
  }

  // G0 A<pos> F<speed>
  if(command.equalsIgnoreCase("G0")){
    if(arg1!=""){
      if(arg1.charAt(0) == AXIS){
        float pos = arg1.substring(1).toFloat();
        if(positioning == "absolute") targetPosition = pos;
        else targetPosition += pos;
      }
      else Serial.println("G0 command for different axis received");
      if(arg2!=""){
        if(arg2.charAt(0) == 'F'){
          float feedrate = arg2.substring(1).toFloat();
  
          /*####### TODO: Set feedrate ##########*/
        }
      }
    }
    else Serial.println("G0 command without arguments received");
  }
  // G90: Absolute Positioning
  else if(command.equalsIgnoreCase("G90")){
    positioning = "absolute";
  }
  // G91: Relative Positioning
  else if(command.equalsIgnoreCase("G91")){
    positioning = "relative";
  }
  // G92 A<pos>: Set the current position to the value specified
  else if(command.equalsIgnoreCase("G92")){
    if(arg1!=""){
      if(arg1.charAt(0) == AXIS){
       position = arg1.substring(1).toFloat(); 
      }
      else Serial.println("G92 command for different axis received");
    }
    else Serial.println("G92 command without arguments received");
  }
  // M92 A<steps>: Set the number of full steps/mm for your setup
  else if(command.equalsIgnoreCase("M92")){
    if(arg1!=""){
      if(arg1.charAt(0) == AXIS){
        stepsPerMM = arg1.substring(1).toFloat();
      }
      else Serial.println("M92 command for different axis received");
    }
    else Serial.println("M92 command without arguments received");
  }
  // M114: Get the current position
  else if(command.equalsIgnoreCase("M114")){
    sendMultipacketCAN(String(position));
  }
  // M503: Report settings
  else if(command.equalsIgnoreCase("M503")){
    reportSettings();
  }
  // M906 A<mA>: Set TMC motor current in mA
  else if(command.equalsIgnoreCase("M906")){
    if(arg1!=""){
      if(arg1.charAt(0) == AXIS){
        float motorCurrent = arg1.substring(1).toFloat();
        driver.rms_current(motorCurrent);        // Set motor RMS current
      }
      else Serial.println("M906 command for different axis received");
    }
    else Serial.println("M906 command without arguments received");
  }
  else {
    Serial.println("Unsupported GCode command received");
    return;
  }
  
}

void reportSettings(){
  
}


void setup(){
  pinMode(LED_0, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);

  pinMode(TMC_EN, OUTPUT);
  pinMode(TMC_MS1, OUTPUT);
  pinMode(TMC_MS2, OUTPUT);
  pinMode(TMC_DIAG, INPUT);
  pinMode(TMC_STEP, OUTPUT);
  pinMode(TMC_DIR, OUTPUT);

  digitalWrite(TMC_EN, HIGH); // disable motor
  
  if(USE_UART){
    // Driver adress 0b00
    digitalWrite(TMC_MS1, LOW);
    digitalWrite(TMC_MS2, LOW);
    
    Serial2.begin(115200);
    
    driver.begin();
    driver.toff(5);                 // Enables driver in software
    driver.rms_current(250);        // Set motor RMS current
    driver.microsteps(MICROSTEPS);  // Set microsteps
    driver.pwm_autoscale(true);     // Needed for stealthChop

    stepper.setMaxSpeed(20000); // 100mm/s @ 80 steps/mm
    stepper.setSpeed(10000);
    stepper.setAcceleration(800000); // 2000mm/s^2
    stepper.setEnablePin(TMC_EN);
    stepper.setPinsInverted(false, false, true);
    stepper.enableOutputs();
  }
  else {

    pinMode(TMC_PDN, OUTPUT);
    digitalWrite(TMC_PDN, LOW);
    
    if(MICROSTEPS == 8){
      digitalWrite(TMC_MS1, LOW);
      digitalWrite(TMC_MS2, LOW);
    }
    else if(MICROSTEPS == 16){
      digitalWrite(TMC_MS1, HIGH);
      digitalWrite(TMC_MS2, HIGH);
    }
    else if(MICROSTEPS == 32){
      digitalWrite(TMC_MS1, HIGH);
      digitalWrite(TMC_MS2, LOW);
    }
    else if(MICROSTEPS == 64){
      digitalWrite(TMC_MS1, LOW);
      digitalWrite(TMC_MS2, HIGH);
    }
    else if(MICROSTEPS > 64){
      digitalWrite(TMC_MS1, LOW);
      digitalWrite(TMC_MS2, HIGH);
    }
    else if(MICROSTEPS < 8){
      digitalWrite(TMC_MS1, LOW);
      digitalWrite(TMC_MS2, LOW);
    }
  }
  

  digitalWrite(LED_0, HIGH);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);

  delay(1000);

  digitalWrite(LED_0, LOW);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  
  
  Serial.begin(115200);
  while (!Serial);

  Serial.print("\nStarting ConfigOnStartup with DoubleResetDetect using " + String(FS_Name));
  Serial.println(" on " + String(ARDUINO_BOARD));
  Serial.println("ESP_WiFiManager Version " + String(ESP_WIFIMANAGER_VERSION));

  Serial.setDebugOutput(false);

  if (FORMAT_FILESYSTEM) FileFS.format();
  if (!FileFS.begin(true)){
    Serial.print(FS_Name);
    Serial.println(F(" failed! AutoFormatting."));
  }

  digitalWrite(LED_0, HIGH);

  unsigned long startedAt = millis();

  ESP_WiFiManager ESP_wifiManager("UniPaste");
  ESP_wifiManager.setMinimumSignalQuality(-1);
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESP_wifiManager.setConfigPortalChannel(0);

  ESP_wifiManager.setCORSHeader("Your Access-Control-Allow-Origin");

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  //Check if there is stored WiFi router/password credentials.
  //If not found, device will remain in configuration mode until switched off via webserver.
  Serial.println("Opening configuration portal.");

  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
    
    ESP_wifiManager.setConfigPortalTimeout(30); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 60s for Config Portal");
  }
  else
  {
    Serial.println("Open Config Portal without Timeout: No stored Credentials.");
    initialConfig = true;
  }

  // SSID to uppercase
  ssid.toUpperCase();

  Serial.println("Starting configuration portal.");
  digitalWrite(LED_0, HIGH);

  // Starts an access point
  if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    Serial.println("Not connected to WiFi but continuing anyway.");
  else
  {
    Serial.println("WiFi connected...yeey :)");
  }

  // Only clear then save data if CP entered and with new valid Credentials
  // No CP => stored getSSID() = ""
  if ( String(ESP_wifiManager.getSSID(0)) != "" && String(ESP_wifiManager.getSSID(1)) != "" )
  {
    // Stored  for later usage, from v1.1.0, but clear first
    memset(&WM_config, 0, sizeof(WM_config));
    
    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      String tempSSID = ESP_wifiManager.getSSID(i);
      String tempPW   = ESP_wifiManager.getPW(i);
  
      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);
  
      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);  
  
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }
  
    saveConfigData();

    initialConfig = true;
  }

  digitalWrite(LED_0, LOW);

  startedAt = millis();

  if (!initialConfig)
  {
    // Load stored data, the addAP ready for MultiWiFi reconnection
    loadConfigData();

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED ) 
    {
      Serial.println("ConnectMultiWiFi in setup");
     
      connectMultiWiFi();
    }
  }

  Serial.print("After waiting ");
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));

  /*******************************************
   **************  CAN Setup  ****************
   *******************************************/
  CAN.setPins(4, 5);
  if(!CAN.begin(125E3)){
    Serial.println("Starting CAN failed");
    while(1);
  }

  CAN.onReceive(onReceive);
   
  //CAN_cfg.speed = CAN_SPEED_125KBPS;
  //CAN_cfg.tx_pin_id = GPIO_NUM_5;
  //CAN_cfg.rx_pin_id = GPIO_NUM_4;
  //CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
  // Init CAN Module
  //ESP32Can.CANInit();

  /*******************************************
   **************  OTA Setup  ****************
   *******************************************/
   
  ArduinoOTA.setHostname("UniPaste");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  //digitalWrite(TMC_EN, LOW); // enable motor
  //digitalWrite(TMC_DIR, LOW); // go forwards

  //stepper.move(100000);
}

void loop(){
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();

  
  
   // Send CAN Message
  if (currentMillis - previousMillis >= 2000) {
    String CANmessage = "Hallo wie geht es?";

    sendMultipacketCAN(CANmessage);
    
    previousMillis = currentMillis;
    /*CAN_frame_t tx_frame;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.MsgID = 0x001;
    String message = "G0 A100 F20.0";
    
    uint8_t msg_length = message.length();
    tx_frame.FIR.B.DLC = msg_length;
    
    for(uint8_t i=0; i<msg_length ; i++){
      tx_frame.data.u8[i] = message.charAt(i);
    }
    tx_frame.data.u8[0] = 'H';
    tx_frame.data.u8[1] = 'e';
    tx_frame.data.u8[2] = 'l';
    tx_frame.data.u8[3] = 'l';
    tx_frame.data.u8[4] = 'o';
    tx_frame.data.u8[5] = 'C';
    tx_frame.data.u8[6] = 'A';
    tx_frame.data.u8[7] = 'N';
    ESP32Can.CANWriteFrame(&tx_frame);*/
  }
  
  //check_status();  
 /* digitalWrite(TMC_STEP,LOW);
  delay(2);
  digitalWrite(TMC_STEP,HIGH);
  delay(2);*/

   /*if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();
        delay(100);
        stepper.move(10000); // Move 100mm
        stepper.enableOutputs();
    }*/
    //stepper.runSpeed();
  
}
