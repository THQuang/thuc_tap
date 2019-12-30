#ifdef SWITCH

int buttonPins[] = BUTTON_PINS;
int relayPins[] = RELAY_PINS;
int sendPin = 14;

Button buttons[CHANNEL_COUNT];
Relay relays[CHANNEL_COUNT];
LED led(LED_PIN);

bool buttonPending[CHANNEL_COUNT] = { false };

// Public functions

void initDevice() {
    // Buttons and relays
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        buttons[i].begin(buttonPins[i], isr);
        buttons[i].setPressCallback(pressCallback);
        buttons[i].setHoldCallback(3000, holdCallback);
        buttons[i].index = i;

        relays[i].begin(relayPins[i], RELAY_OFF);
    }

    // LED
    led.begin(LED_OFF);    
}

void loopDevice() {
    led.loop();
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        buttons[i].loop();

        if (buttonPending[i]) {
            stopWiFiConfig();
            syncSwitchState(i);
            buttonPending[i] = false;            
        }
    }
}

String deviceDefinition() {
    DynamicJsonBuffer jsonBuffer;
    JsonArray& result = jsonBuffer.createArray();
    String content;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        JsonObject& item = jsonBuffer.createObject();
        item.set("entity", String(i));
        item.set("type", "switch");
        item.set("name", "Switch " + String(i + 1));
        result.add(item);            
    }
    result.printTo(content);
    return content;
}

void syncDeviceState() {
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        syncSwitchState(i);
    }
}

void indicateConfigMode() {
    led.blink(250);    
}

void indicateNormalMode() {
    led.blink(0);
}

void indicateDisconnected() {
    led.blink(1000);
}

void receiveMessage(char* topic, byte* payload, unsigned int length) {
    logi("Message: %s", topic);
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        String setTopic = group + "/" + number + "/" + String(i) + "/set";
        if (setTopic == topic) {
            if (strncmp("on", (char*)payload, length) == 0) {
                relays[i].set(RELAY_ON);
            }
            else if (strncmp("off", (char*)payload, length) == 0) {
                relays[i].set(RELAY_OFF);
            }
            syncSwitchState(i);
        }
    }
}


// Private functions

void syncSwitchState(int index) {
    String stateTopic = group + "/" + number + "/" + String(index) + "/state";
    MX.client.publish(stateTopic.c_str(), relays[index].get() == RELAY_ON ? "on" : "off", true);
}

void isr() {
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        buttons[i].isr();
    }
}

void pressCallback(Button& button) {
    int index = button.index;
    if (relays[index].get() == RELAY_OFF) {
        relays[index].set(RELAY_ON);
    }
    else {
        relays[index].set(RELAY_OFF);
    }
    buttonPending[index] = true;
}

void holdCallback(Button& button) {
    startWiFiConfig();
}
void irsendraw(uint16_t *data){
//    IRsend irsend(sendPin);
//    irsend.begin();
    irsend.sendRaw(data, sizeof(data) / sizeof(data[0]), 38);
}

String dumpACInfo(decode_results *results) {
  String description = "";
#if DECODE_DAIKIN
  if (results->decode_type == DAIKIN) {
    IRDaikinESP ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_DAIKIN
#if DECODE_DAIKIN2
  if (results->decode_type == DAIKIN2) {
    IRDaikin2 ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_DAIKIN2
#if DECODE_FUJITSU_AC
  if (results->decode_type == FUJITSU_AC) {
    IRFujitsuAC ac(0);
    ac.setRaw(results->state, results->bits / 8);
    description = ac.toString();
  }
#endif  // DECODE_FUJITSU_AC
#if DECODE_KELVINATOR
  if (results->decode_type == KELVINATOR) {
    IRKelvinatorAC ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_KELVINATOR
#if DECODE_MITSUBISHI_AC
  if (results->decode_type == MITSUBISHI_AC) {
    IRMitsubishiAC ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_MITSUBISHI_AC
#if DECODE_TOSHIBA_AC
  if (results->decode_type == TOSHIBA_AC) {
    IRToshibaAC ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_TOSHIBA_AC
#if DECODE_GREE
  if (results->decode_type == GREE) {
    IRGreeAC ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_GREE
#if DECODE_MIDEA
  if (results->decode_type == MIDEA) {
    IRMideaAC ac(0);
    ac.setRaw(results->value);  // Midea uses value instead of state.
    description = ac.toString();
  }
#endif  // DECODE_MIDEA
#if DECODE_HAIER_AC
  if (results->decode_type == HAIER_AC) {
    IRHaierAC ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_HAIER_AC
#if DECODE_HAIER_AC_YRW02
  if (results->decode_type == HAIER_AC_YRW02) {
    IRHaierACYRW02 ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_HAIER_AC_YRW02
#if DECODE_SAMSUNG_AC
  if (results->decode_type == SAMSUNG_AC) {
    IRSamsungAc ac(0);
    ac.setRaw(results->state, results->bits / 8);
    description = ac.toString();
  }
#endif  // DECODE_SAMSUNG_AC
#if DECODE_COOLIX
  if (results->decode_type == COOLIX) {
    IRCoolixAC ac(0);
    ac.setRaw(results->value);  // Coolix uses value instead of state.
    description = ac.toString();
  }
#endif  // DECODE_COOLIX
#if DECODE_PANASONIC_AC
  if (results->decode_type == PANASONIC_AC &&
      results->bits > kPanasonicAcShortBits) {
    IRPanasonicAc ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_PANASONIC_AC
#if DECODE_HITACHI_AC
  if (results->decode_type == HITACHI_AC) {
    IRHitachiAc ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_HITACHI_AC
#if DECODE_WHIRLPOOL_AC
  if (results->decode_type == WHIRLPOOL_AC) {
    IRWhirlpoolAc ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_WHIRLPOOL_AC
#if DECODE_VESTEL_AC
  if (results->decode_type == VESTEL_AC) {
    IRVestelAc ac(0);
    ac.setRaw(results->value);  // Like Coolix, use value instead of state.
    description = ac.toString();
  }
#endif  // DECODE_VESTEL_AC
#if DECODE_TECO
  if (results->decode_type == TECO) {
    IRTecoAc ac(0);
    ac.setRaw(results->value);  // Like Coolix, use value instead of state.
    description = ac.toString();
  }
#endif  // DECODE_TECO
#if DECODE_TCL112AC
  if (results->decode_type == TCL112AC) {
    IRTcl112Ac ac(0);
    ac.setRaw(results->state);
    description = ac.toString();
  }
#endif  // DECODE_TCL112AC
  // If we got a human-readable description of the message, display it.
//  if (description != "") Serial.println("description);
  return description;
}

char* UpperCase_P(char* dest, const char* source)
{
  char* write = dest;
  const char* read = source;
  char ch = '.';
  while (ch != '\0') {
    ch = pgm_read_byte(read++);
    *write++ = toupper(ch);
  }
  return dest;
}
#endif
