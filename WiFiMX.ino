#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Log.h>
#include <MX.h>
#include <API.h>
#include <Flash.h>
#include <LED.h>
#include <Relay.h>
#include <Button.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include<stdlib.h>
#include <cstdlib>
#include <ProntoHex.h>



// thư viện ir
#include <Arduino.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>
// The following are only needed for extended decoding of A/C Messages
ProntoHex ph = ProntoHex();
// ac state
stdAc::state_t state;  // Where we will store the desired state of the A/C.
stdAc::state_t prev;

IRac ac(14);
IRsend irsend(14);

#if DECODE_AC
const uint8_t kTimeout = 50;
#else   // DECODE_AC
const uint8_t kTimeout = 15;
#endif  // DECODE_AC
const uint16_t kMinUnknownSize = 12;
#define LEGACY_TIMING_INFO false
IRrecv irrecv(5, 1024, kTimeout, true);
decode_results results; 
// hết thư viện ir

#include "Hardware.h"
#include "Definition.h"

#define MODEL "MXSW"
#define BAUD_RATE 115200

#define FLASH_SIGN  0xA5
#define CHUNK_CONFIG    0xEE
#define CHUNK_COUNT     1

#define null (nullptr)

struct SystemConfig {
    char group[16];
    char key[32];
};

struct Schedule {
    uint32_t set: 1;
    uint32_t enabled: 1;
    uint32_t time: 11;
    uint32_t recurrent: 7;
    uint32_t index: 4;
    uint32_t value: 7;    
};

byte mac[6];
char host[33];
char number[33];

String group;
String key;

bool connected = false;
bool first = true;
bool configuring = false;


// các hàm else if ir


void setup() {
    delay(2000);
    Serial.begin(BAUD_RATE);
    Serial.println();

    WiFi.macAddress(mac);
    snprintf(host, sizeof(host), "%s_%02X%02X%02X", MODEL, mac[3], mac[4], mac[5]);
    snprintf(number, sizeof(number), "MX%02X%02X%02X%02X%02X%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // System
    Flash.begin(loadCallback, saveCallback);
    Flash.load();
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    // Device
    initDevice();

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.hostname(host);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin();

    // API
    API.begin(number);
    API.setDefineCallback(defineCallback);
    API.setSetupCallback(setupCallback);

    // MX
    MX.begin("mx.javis.io", 8883);
    MX.setConnectCallback(connectCallback);
    MX.setMessageCallback(messageCallback);
    MX.setUser(group, number, key);

    // OTA
    ArduinoOTA.begin();

    // Start app
    Serial.println();
    Serial.print("Number: ");
    Serial.println(number);
    
    //IR rect
irsend.begin();
#if DECODE_HASH
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif
irrecv.enableIRIn();
// send ir
    state.protocol = decode_type_t::UNKNOWN;
    state.model = 1;  // Some A/C's have different models. Let's try using just 1.
    state.mode = stdAc::opmode_t::kAuto;  // Run in cool mode initially.
    state.power = false;  // Initially start with the unit off.
    state.celsius = true;  // Use Celsius for units of temp. False = Fahrenheit
    state.degrees = 21.0f;  // 21 degrees.
    state.fanspeed = stdAc::fanspeed_t::kMedium;  // Start with the fan at medium.
    state.swingv = stdAc::swingv_t::kOff;  // Don't swing the fan up or down.
    state.swingh = stdAc::swingh_t::kOff;  // Don't swing the fan left or right.
    state.light = false;  // Turn off any LED/Lights/Display that we can.
    state.beep = false;  // Turn off any beep from the A/C if we can.
    state.econo = false;  // Turn off any economy modes if we can.
    state.filter = false;  // Turn off any Ion/Mold/Health filters if we can.
    state.turbo = false;  // Don't use any turbo/powerful/etc modes.
    state.quiet = false;  // Don't use any quiet/silent/etc modes.
    state.sleep = -1;  // Don't set any sleep time or modes.
    state.clean = false;  // Turn off any Cleaning options if we can.
    state.clock = -1;  // Don't set any current time if we can avoid it.
}

void loop() {
    if (configuring) {
        if (WiFi.smartConfigDone()) {
            configuring = false;
            indicateNormalMode();
            MX.disable();
            API.unlock();
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if (!connected) {
            if (first) {
                logi("WiFi connected after reboot. IP: %s", WiFi.localIP().toString().c_str());
                first = false;
            }
            else {
                logi("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
            }
            connected = true;
            indicateNormalMode();            
        }
        MX.loop();
        API.loop();
        ArduinoOTA.handle();
    }
    else if (WiFi.status() != WL_CONNECTED) {
        if (connected) {
            connected = false;
            if (!configuring) {
                indicateDisconnected();
            }
        }
    }

    loopDevice();
    ir_rect();
 
}

void startWiFiConfig(void) {
    if (configuring) {
        return;
    }

    if (!WiFi.beginSmartConfig()) {
        return;
    }

    configuring = true;
    indicateConfigMode();
}

void stopWiFiConfig(void) {
    if (!configuring) {
        return;
    }
    
    WiFi.stopSmartConfig();
    WiFi.begin();
    configuring = false;
    indicateNormalMode();
}

void loadCallback() {
    SystemConfig config = { 0 };
    
    uint8_t sign = Flash.readUint8();
    if (sign != FLASH_SIGN) {
        return;
    }

    int count = Flash.readUint8();
    for (int index = 0; index < count; index++) {
        int id = Flash.readUint8();
        int size = Flash.readUint8();
        switch (id) {
        case CHUNK_CONFIG:
            Flash.read(size, &config, sizeof(config));
            group = config.group;
            key = config.key;                
            break;
        default:
            Flash.skip(size);
            break;
        }
    }
}

void saveCallback() {        
    // Header
    Flash.writeUint8(FLASH_SIGN);
    Flash.writeUint8(CHUNK_COUNT); // chunk count

    // System config chunk
    SystemConfig config = { 0 };
    snprintf(config.group, sizeof(config.group), "%s", group.c_str());
    snprintf(config.key, sizeof(config.key), "%s", key.c_str());
    
    Flash.writeUint8(CHUNK_CONFIG);
    Flash.writeUint8(sizeof(config));
    Flash.write(&config, sizeof(config));
}    

String defineCallback() {
    return deviceDefinition();
}

void setupCallback(String& _group, String& _key) {
    group = _group;
    key = _key;
    Flash.save();
    MX.setUser(group, number, key);
    MX.enable();
}

void connectCallback() {
    syncDeviceState();
    String topic = group + "/" + number + "/#";
    MX.client.subscribe(topic.c_str(), 1);
    logi("Subscribe: %s", topic.c_str());
    Serial.println();
    Serial.print("Sub:");
    Serial.print(topic);
    Serial.print(".....");
}

void messageCallback(char* topic, byte* payload, unsigned int length) {
    Irsend_raw(topic, payload, length);
    receiveMessage(topic, payload, length);
    Irsend_data(topic, payload, length);
    Irsend_HVAC(topic, payload, length);

}
void Irsend_data(char* topic, byte* payload, unsigned int length) {
    logi("Message: %s", topic);
    String setTopic = group + "/" + number + "/" + "ir" + "/irsend";
    if (setTopic == topic){
        irrecv.disableIRIn();
        String data;
        for(int i=0; i<length; i++){
            data=data+(char)payload[i];
        }
        data.trim();
        if (data.startsWith("0000 ")) // Then we assume it is Pronto Hex
        {
              ph.convert(data);
              uint16_t a[ph.length];
              for(int i=0; i < ph.length ; i++){
                  a[i]= (uint16_t)ph.convertedRaw[i];
              }
          
              irsend.sendRaw(a, ph.length, ph.frequency); 
         }
        irrecv.enableIRIn();
    }
}

void Irsend_HVAC(char* topic, byte* payload, unsigned int length) {
    logi("Message: %s", topic);
    String setTopic = group + "/" + number + "/" + "ir" + "/HVAC";
    if (setTopic == topic){
      char data[length];
      for(int i = 0 ; i < length ; i++){
        data[i]=payload[i];
        }    
      IrRemoteCmndIrHvacJson(data);
}
}

void Irsend_raw(char* topic, byte* payload, unsigned int length) {
    logi("Message: %s", topic);
    String setTopic = group + "/" + number + "/" + "ir" + "/irraw";
    if (setTopic == topic){
          irrecv.disableIRIn();
          Serial.println(length);
          int sum=0;
          for(int i = 0 ; i < length ; i++){  
          if((char)payload[i] == ',')
                sum++;
            }
          sum++;
          uint16_t In[sum];
          Serial.println(sum);
          char *p;
          p = strtok((char*)payload , "{}, "); //cat chuoi bang cac ky tu , va space
          for(int index=0 ; index < sum ; index++)
          {         
                    In[index]=(uint16_t)convert(p);
                    p = strtok(NULL, "{}, "); //cat chuoi tu vi tri dung lai truoc do
          }
          Serial.println("ok");      
          irsend.sendRaw(In, sizeof(In) / sizeof(In[0]), 38);
          irrecv.enableIRIn();
      }
}

int convert(char t[]){
  int n;
  int sum=0;
  n = strlen(t);
  for (int i=0;i<n;i++){
    sum = sum*10 + ((int) t[i] - 48);
  }
  return sum;
}

void ir_rect(){
if (irrecv.decode(&results)) {
    String irstaterawTopic = group + "/" + number + "/" + "state/ir";
    String irstateTopic = group + "/" + number + "/" + "state/ir/state";
    
    MX.client.publish(irstaterawTopic.c_str(),resultToRawCode(&results).c_str(), true);
    yield();
    String description = dumpACInfo(&results);
    if (description == ""){MX.client.publish(irstateTopic.c_str(),"UNKNOW", true);}
    else{MX.client.publish(irstateTopic.c_str(),description.c_str(), true);}
//    dumpACInfo(&results);
    yield();
    }
}

enum IrErrors { IE_RESPONSE_PROVIDED, IE_NO_ERROR, IE_INVALID_RAWDATA, IE_INVALID_JSON, IE_SYNTAX_IRSEND, IE_SYNTAX_IRHVAC,
                IE_UNSUPPORTED_HVAC, IE_UNSUPPORTED_PROTOCOL };
const char kIrRemoteCommands[] PROGMEM = "|" D_CMND_IRHVAC "|" D_CMND_IRSEND ; // No prefix
//void (* const IrRemoteCommand[])(void) PROGMEM = { &CmndIrHvac, &CmndIrSend };
const stdAc::fanspeed_t IrHvacFanSpeed[] PROGMEM =  { stdAc::fanspeed_t::kAuto,
                      stdAc::fanspeed_t::kMin, stdAc::fanspeed_t::kLow,stdAc::fanspeed_t::kMedium,
                      stdAc::fanspeed_t::kHigh, stdAc::fanspeed_t::kMax };
uint8_t reverseBitsInByte(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
// reverse bits in each byte
uint64_t reverseBitsInBytes64(uint64_t b) {
  union {
    uint8_t b[8];
    uint64_t i;
  } a;
  a.i = b;
  for (uint32_t i=0; i<8; i++) {
    a.b[i] = reverseBitsInByte(a.b[i]);
  }
  return a.i;
}
// list all supported protocols, either for IRSend or for IRHVAC, separated by '|'
String listSupportedProtocols(bool hvac) {
  String l("");
  bool first = true;
  for (uint32_t i = UNUSED + 1; i <= kLastDecodeType; i++) {
    bool found = false;
    if (hvac) {
      found = IRac::isProtocolSupported((decode_type_t)i);
    } else {
      found = (IRsend::defaultBits((decode_type_t)i) > 0) && (!IRac::isProtocolSupported((decode_type_t)i));
    }
    if (found) {
      if (first) {
        first = false;
      } else {
        l += "|";
      }
      l += typeToString((decode_type_t)i);
    }
  }
  return l;
}
String sendACJsonState(const stdAc::state_t &state) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json[D_JSON_IRHVAC_VENDOR] = typeToString(state.protocol);
  json[D_JSON_IRHVAC_MODEL] = state.model;
  json[D_JSON_IRHVAC_POWER] = IRac::boolToString(state.power);
  json[D_JSON_IRHVAC_MODE] = IRac::opmodeToString(state.mode);
  // Home Assistant wants mode to be off if power is also off & vice-versa.
  if (state.mode == stdAc::opmode_t::kOff || !state.power) {
    json[D_JSON_IRHVAC_MODE] = IRac::opmodeToString(stdAc::opmode_t::kOff);
    json[D_JSON_IRHVAC_POWER] = IRac::boolToString(false);
  }
  json[D_JSON_IRHVAC_CELSIUS] = IRac::boolToString(state.celsius);
  if (floorf(state.degrees) == state.degrees) {
    json[D_JSON_IRHVAC_TEMP] = floorf(state.degrees);       // integer
  } //else
//    json[D_JSON_IRHVAC_TEMP] = RawJson(String(state.degrees, 1));  // non-integer, limit to only 1 sub-digit
//  }
  json[D_JSON_IRHVAC_FANSPEED] = IRac::fanspeedToString(state.fanspeed);
  json[D_JSON_IRHVAC_SWINGV] = IRac::swingvToString(state.swingv);
  json[D_JSON_IRHVAC_SWINGH] = IRac::swinghToString(state.swingh);
  json[D_JSON_IRHVAC_QUIET] = IRac::boolToString(state.quiet);
  json[D_JSON_IRHVAC_TURBO] = IRac::boolToString(state.turbo);
  json[D_JSON_IRHVAC_ECONO] = IRac::boolToString(state.econo);
  json[D_JSON_IRHVAC_LIGHT] = IRac::boolToString(state.light);
  json[D_JSON_IRHVAC_FILTER] = IRac::boolToString(state.filter);
  json[D_JSON_IRHVAC_CLEAN] = IRac::boolToString(state.clean);
  json[D_JSON_IRHVAC_BEEP] = IRac::boolToString(state.beep);
  json[D_JSON_IRHVAC_SLEEP] = state.sleep;
  String payload = "";
  payload.reserve(200);
  json.printTo(payload);
  return payload;
}
uint32_t IrRemoteCmndIrHvacJson(char *payload)
{
  char parm_uc[12];
  if (strlen(payload) < 8) { return IE_INVALID_JSON; }
  DynamicJsonBuffer jsonBuf;
  JsonObject &json = jsonBuf.parseObject(payload);
  if (!json.success()) { return IE_INVALID_JSON; }
  // from: https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/CommonAcControl/CommonAcControl.ino
  state.protocol = decode_type_t::UNKNOWN;
  state.model = 1;  // Some A/C's have different models. Let's try using just 1.
  state.mode = stdAc::opmode_t::kAuto;  // Run in cool mode initially.
  state.power = false;  // Initially start with the unit off.
  state.celsius = true;  // Use Celsius for units of temp. False = Fahrenheit
  state.degrees = 21.0f;  // 21 degrees.
  state.fanspeed = stdAc::fanspeed_t::kMedium;  // Start with the fan at medium.
  state.swingv = stdAc::swingv_t::kOff;  // Don't swing the fan up or down.
  state.swingh = stdAc::swingh_t::kOff;  // Don't swing the fan left or right.
  state.light = false;  // Turn off any LED/Lights/Display that we can.
  state.beep = false;  // Turn off any beep from the A/C if we can.
  state.econo = false;  // Turn off any economy modes if we can.
  state.filter = false;  // Turn off any Ion/Mold/Health filters if we can.
  state.turbo = false;  // Don't use any turbo/powerful/etc modes.
  state.quiet = false;  // Don't use any quiet/silent/etc modes.
  state.sleep = -1;  // Don't set any sleep time or modes.
  state.clean = false;  // Turn off any Cleaning options if we can.
  state.clock = -1;  // Don't set any current time if we can avoid it.
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_VENDOR));
  if (json.containsKey(parm_uc)) { state.protocol = strToDecodeType(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_PROTOCOL));
  if (json.containsKey(parm_uc)) { state.protocol = strToDecodeType(json[parm_uc]); }   // also support 'protocol'
  if (decode_type_t::UNKNOWN == state.protocol) { return IE_UNSUPPORTED_HVAC; }
  if (!IRac::isProtocolSupported(state.protocol)) { return IE_UNSUPPORTED_HVAC; }
  // for fan speed, we also support 1-5 values
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_FANSPEED));
  if (json.containsKey(parm_uc)) {
    uint32_t fan_speed = json[parm_uc];
    if ((fan_speed >= 1) && (fan_speed <= 5)) {
      state.fanspeed = (stdAc::fanspeed_t) pgm_read_byte(&IrHvacFanSpeed[fan_speed]);
    } else {
      state.fanspeed = IRac::strToFanspeed(json[parm_uc]);
    }
  }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_MODEL));
  if (json.containsKey(parm_uc)) { state.model = IRac::strToModel(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_MODE));
  if (json.containsKey(parm_uc)) { state.mode = IRac::strToOpmode(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_SWINGV));
  if (json.containsKey(parm_uc)) { state.swingv = IRac::strToSwingV(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_SWINGH));
  if (json.containsKey(parm_uc)) { state.swingh = IRac::strToSwingH(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_TEMP));
  if (json.containsKey(parm_uc)) { state.degrees = json[parm_uc]; }
  // AddLog_P2(LOG_LEVEL_DEBUG, PSTR("model %d, mode %d, fanspeed %d, swingv %d, swingh %d"),
  //             state.model, state.mode, state.fanspeed, state.swingv, state.swingh);
  // decode booleans
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_POWER));
  if (json.containsKey(parm_uc)) { state.power = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_CELSIUS));
  if (json.containsKey(parm_uc)) { state.celsius = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_LIGHT));
  if (json.containsKey(parm_uc)) { state.light = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_BEEP));
  if (json.containsKey(parm_uc)) { state.beep = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_ECONO));
  if (json.containsKey(parm_uc)) { state.econo = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_FILTER));
  if (json.containsKey(parm_uc)) { state.filter = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_TURBO));
  if (json.containsKey(parm_uc)) { state.turbo = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_QUIET));
  if (json.containsKey(parm_uc)) { state.quiet = IRac::strToBool(json[parm_uc]); }
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_CLEAN));
  if (json.containsKey(parm_uc)) { state.clean = IRac::strToBool(json[parm_uc]); }
  // optional timer and clock
  UpperCase_P(parm_uc, PSTR(D_JSON_IRHVAC_SLEEP));
  if (json[parm_uc]) { state.sleep = json[parm_uc]; }
  //if (json[D_JSON_IRHVAC_CLOCK]) { state.clock = json[D_JSON_IRHVAC_CLOCK]; }   // not sure it's useful to support 'clock'
  irrecv.disableIRIn();
  delay(10);
  bool success = ac.sendAc(state, &prev);
  if (success == true) {
      Serial.println("Success");
      Serial.println(sendACJsonState(state));
    }else{
      return IE_SYNTAX_IRHVAC;
    }
  irrecv.enableIRIn();
}
