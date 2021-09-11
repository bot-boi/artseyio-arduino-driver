// https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/examples/Peripheral/hid_keyboard/hid_keyboard.ino
// https://github.com/artseyio/artsey
#include <bluefruit.h>
#include "bluetooth.h"


/* ============================== user configuration ============================== */
// REVIEW: these could be macros
// bool mode_left = true;  // lefthanded or no
int press_delay = 50;   // wait N ms after the initial press before reading the state of the keys
int release_delay = 0;  // wait N ms after release
int poll_delay_ms = 0;  // wait N ms every tick
int hold_time_ms = 800;    // how long it takes for a held key to be HOLD instead of TAP

// maps pins (the elements) to their corresponding values (the indicies of a keymap)
int pins[8] = {11, 7, 15, 16, // <--- left handed
               29, 31, 30, 27};
//int pins[8] = {16, 15, 7, 11,  <--- right handed
//               27, 30, 31, 29};  
// right = {'a', 'r', 't', 's',
//          'e', 'y', 'i', 'o'};
// left  = {'s', 't', 'r', 'a',
//          'o', 'i', 'y', 'e'};

enum PRESS_TYPE {
  TAP,
  HOLD
};

typedef int combo_action(struct Combo combo);

typedef struct Combo {
  uint8_t keycode;
  PRESS_TYPE press_type; // 0: press, 1: hold
  uint8_t output;
  uint8_t modifier;
  combo_action *func;
} Combo;


uint8_t current_modifier = 0;
uint8_t current_modifier_locked = 0;
Combo *current_layer_ptr;
int current_layer_len = 0;

int do_nothing(Combo combo) { // nop
  return 0;
}

int send_key(Combo combo) {
  ble_press_key(combo.output, combo.modifier | current_modifier | current_modifier_locked);
  current_modifier = 0;
  return 0;
}

int accumulate_modifier(Combo combo) {
  /* Apply combo modifier to the next key press. */
  current_modifier |= combo.modifier;
  return 0;
}

int toggle_modifier(Combo combo) {
  /* Apply modifier to every key press, or not. */
  current_modifier_locked ^= combo.modifier;
  return 0;
}

#define MSTEP 15
int mouse_move_up(Combo combo) {
  blehid.mouseMove(0, -MSTEP);
  return 0;
}
int mouse_move_left(Combo combo) {
  blehid.mouseMove(-MSTEP, 0);
  return 0;
}
int mouse_move_down(Combo combo) {
  blehid.mouseMove(0, MSTEP);
  return 0;
}
int mouse_move_right(Combo combo) {
  blehid.mouseMove(MSTEP, 0);
  return 0;
}
// TODO
int mouse_scroll_up(Combo combo) {
  return 0;
}
int mouse_scroll_down(Combo combo) {
  return 0;
}
int mouse_click_left(Combo combo) {
  blehid.mouseButtonPress(MOUSE_BUTTON_LEFT);
  blehid.mouseButtonRelease();
  return 0;
}
int mouse_click_right(Combo combo) {
  blehid.mouseButtonPress(MOUSE_BUTTON_RIGHT);
  blehid.mouseButtonRelease();
  return 0;
}

// global combos, meant to be present in every layer
#define ENTER           {238, TAP, HID_KEY_ENTER,        NULL,                        send_key},
#define BACKSPACE       {237, TAP, HID_KEY_BACKSPACE,    NULL,                        send_key},
#define CTRL_LEFT_ONCE  {231, TAP, HID_KEY_NONE,         KEYBOARD_MODIFIER_LEFTCTRL,  accumulate_modifier},
#define SHIFT_LEFT_ONCE {225, TAP, HID_KEY_NONE,         KEYBOARD_MODIFIER_LEFTSHIFT, accumulate_modifier},  
#define SHIFT_LEFT_LOCK {221, TAP, HID_KEY_NONE,         KEYBOARD_MODIFIER_LEFTSHIFT, toggle_modifier},      
#define GUI_LEFT_ONCE   {215, TAP, HID_KEY_NONE,         KEYBOARD_MODIFIER_LEFTGUI,   accumulate_modifier},  // aka super key?
#define DELETE          {189, TAP, HID_KEY_DELETE,       NULL,                        send_key},
#define ALT_LEFT_ONCE   {183, TAP, HID_KEY_NONE,         KEYBOARD_MODIFIER_LEFTALT,   accumulate_modifier},
#define ESCAPE          {124, TAP, HID_KEY_ESCAPE,       NULL,                        send_key},
#define TAB             {120, TAP, HID_KEY_TAB,          NULL,                        send_key},
#define SPACE           {15,  TAP, HID_KEY_SPACE,        NULL,                        send_key},
#define GLOBAL_COMBOS \
          ENTER \
          BACKSPACE \
          CTRL_LEFT_ONCE \
          SHIFT_LEFT_ONCE \
          SHIFT_LEFT_LOCK \
          GUI_LEFT_ONCE \
          DELETE \
          ALT_LEFT_ONCE \
          ESCAPE \
          TAB \
          SPACE

struct Combo layer_numbers[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_1,            NULL,                        send_key},
  {253, TAP, HID_KEY_2,            NULL,                        send_key},
  {252, TAP, HID_KEY_7,            NULL,                        send_key},
  {251, TAP, HID_KEY_3,            NULL,                        send_key},
  {249, TAP, HID_KEY_8,            NULL,                        send_key},
  {247, TAP, HID_KEY_NONE,         NULL,                        set_layer_primary},
  {239, TAP, HID_KEY_4,            NULL,                        send_key},
  {223, TAP, HID_KEY_5,            NULL,                        send_key},
  {207, TAP, HID_KEY_9,            NULL,                        send_key},
  {191, TAP, HID_KEY_6,            NULL,                        send_key},
  {159, TAP, HID_KEY_0,            NULL,                        send_key}
};
int layer_numbers_len = sizeof layer_numbers / sizeof layer_numbers[0];

struct Combo layer_brackets[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_NONE,         NULL,                        set_layer_primary},
  {253, TAP, HID_KEY_0,            KEYBOARD_MODIFIER_LEFTSHIFT, send_key},  // )
  {251, TAP, HID_KEY_9,            KEYBOARD_MODIFIER_LEFTSHIFT, send_key},  // (
  {247, TAP, HID_KEY_BRACKET_RIGHT,KEYBOARD_MODIFIER_LEFTSHIFT, send_key},  // }
  {223, TAP, HID_KEY_BRACKET_RIGHT,NULL,                        send_key},
  {191, TAP, HID_KEY_BRACKET_LEFT ,NULL,                        send_key},
  {127, TAP, HID_KEY_BRACKET_LEFT ,KEYBOARD_MODIFIER_LEFTSHIFT, send_key}  // {
};
int layer_brackets_len = sizeof layer_brackets / sizeof layer_brackets[0];

struct Combo layer_symbols[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_1,            KEYBOARD_MODIFIER_LEFTSHIFT, send_key},  // !
  {253, TAP, HID_KEY_BACKSLASH,    NULL,                        send_key},
  {251, TAP, HID_KEY_SEMICOLON,    NULL,                        send_key},
  {247, TAP, HID_KEY_GRAVE,        NULL,                        send_key},
  {239, TAP, HID_KEY_NONE,         NULL,                        set_layer_primary},
  {223, TAP, HID_KEY_SLASH,        KEYBOARD_MODIFIER_LEFTSHIFT, send_key},
  {191, TAP, HID_KEY_MINUS,        NULL,                        send_key},
  {127, TAP, HID_KEY_EQUAL,        NULL,                        send_key}
};
int layer_symbols_len = sizeof layer_symbols / sizeof layer_symbols[0];

struct Combo layer_navigation[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_END,          NULL,                        send_key},
  {253, TAP, HID_KEY_ARROW_UP,     NULL,                        send_key},
  {251, TAP, HID_KEY_HOME,         NULL,                        send_key},
  {247, TAP, HID_KEY_PAGE_UP,      NULL,                        send_key},
  {239, TAP, HID_KEY_ARROW_RIGHT,  NULL,                        send_key},
  {223, TAP, HID_KEY_ARROW_DOWN,   NULL,                        send_key},
  {191, TAP, HID_KEY_ARROW_LEFT,   NULL,                        send_key},
  {173, TAP, HID_KEY_NONE,         NULL,                        set_layer_primary},
  {127, TAP, HID_KEY_PAGE_DOWN,    NULL,                        send_key},
};
int layer_navigation_len = sizeof layer_navigation / sizeof layer_navigation[0];

struct Combo layer_mouse[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_NONE,         NULL,                        mouse_click_left},
  {253, TAP, HID_KEY_NONE,         NULL,                        mouse_move_up},
  {251, TAP, HID_KEY_NONE,         NULL,                        mouse_click_right},
  {247, TAP, HID_KEY_NONE,         NULL,                        mouse_scroll_up},
  {239, TAP, HID_KEY_NONE,         NULL,                        mouse_move_right},
  {223, TAP, HID_KEY_NONE,         NULL,                        mouse_move_down},
  {218, TAP, HID_KEY_NONE,         NULL,                        set_layer_primary},
  {191, TAP, HID_KEY_NONE,         NULL,                        mouse_move_left},
  {127, TAP, HID_KEY_NONE,         NULL,                        mouse_scroll_up},
};
int layer_mouse_len = sizeof layer_mouse / sizeof layer_mouse[0];

struct Combo layer_primary[] = {
  GLOBAL_COMBOS
  {254, TAP, HID_KEY_A,            NULL,			                  send_key},
  {254, HOLD, HID_KEY_A,           NULL,                        set_layer_brackets},
  {253, TAP, HID_KEY_R,            NULL,			                  send_key},
  {252, TAP, HID_KEY_F,            NULL,			                  send_key},
  {251, TAP, HID_KEY_T,            NULL,			                  send_key},
  {249, TAP, HID_KEY_G,            NULL,			                  send_key},
  {248, TAP, HID_KEY_D,            NULL,			                  send_key},
  {247, TAP, HID_KEY_S,            NULL,			                  send_key},
  {247, HOLD, HID_KEY_NONE,        NULL,                        set_layer_numbers},
  {246, TAP, HID_KEY_W,            NULL,			                  send_key},
  {245, TAP, HID_KEY_V,            NULL,			                  send_key},
  {243, TAP, HID_KEY_J,            NULL,			                  send_key},
  {242, TAP, HID_KEY_Q,            NULL,                        send_key},
  {241, TAP, HID_KEY_X,            NULL,			                  send_key},
  {240, TAP, HID_KEY_Z,            NULL,			                  send_key},
  {239, TAP, HID_KEY_E,            NULL,			                  send_key},
  {239, HOLD, HID_KEY_NONE,         NULL,                       set_layer_symbols},
  {223, TAP, HID_KEY_Y,            NULL,			                  send_key},
  {222, TAP, HID_KEY_COMMA,        NULL,			                  send_key},
  {218, TAP, HID_KEY_NONE,         NULL,                        set_layer_mouse},
  {207, TAP, HID_KEY_C,            NULL,			                  send_key},
  {191, TAP, HID_KEY_I,            NULL,			                  send_key},
  {190, TAP, HID_KEY_PERIOD,       NULL,			                  send_key},
  {187, TAP, HID_KEY_1,            KEYBOARD_MODIFIER_LEFTSHIFT, send_key},  // !
  {175, TAP, HID_KEY_H,            NULL,			                  send_key},
  {173, TAP, HID_KEY_NONE,         NULL,                        set_layer_navigation},
  {159, TAP, HID_KEY_U,            NULL,			                  send_key},
  {158, TAP, HID_KEY_APOSTROPHE,   NULL,			                  send_key},
  {143, TAP, HID_KEY_L,            NULL,			                  send_key},
  {127, TAP, HID_KEY_O,            NULL,			                  send_key},
  {126, TAP, HID_KEY_SLASH,        NULL,			                  send_key},
  {111, TAP, HID_KEY_B,            NULL,			                  send_key},
  {95,  TAP, HID_KEY_K,            NULL,			                  send_key},
  {63,  TAP, HID_KEY_N,            NULL,			                  send_key},
  {47,  TAP, HID_KEY_P,            NULL,			                  send_key},
  {31,  TAP, HID_KEY_M,            NULL,			                  send_key},
  {30,  TAP, HID_KEY_CAPS_LOCK,    NULL,			                  send_key},

};
int layer_primary_len = sizeof layer_primary / sizeof layer_primary[0];

int set_layer_primary(Combo combo) {
  /* Set current layer to primary ( artseyio ) */
  current_layer_ptr = layer_primary;
  current_layer_len = layer_primary_len;
  return 0;
}

int set_layer_numbers(Combo combo) {
  /* Set current layer to numbers ( 1234567890 ) */
  current_layer_ptr = layer_numbers;
  current_layer_len = layer_numbers_len;
  return 0;
}

int set_layer_brackets(Combo combo) {
  /* Set current layer to brackets ( []{}() ) */
  current_layer_ptr = layer_brackets;
  current_layer_len = layer_brackets_len;
  return 0;
}

int set_layer_symbols(Combo combo) {
  /* Set current layer to symbols ( !\;~?-= ) */
  current_layer_ptr = layer_symbols;
  current_layer_len = layer_symbols_len;
  return 0;
}

int set_layer_navigation(Combo combo) {
  /* Set current layer to navigation ( arrow keys, pgup/down, home, end ) */
  current_layer_ptr = layer_navigation;
  current_layer_len = layer_navigation_len;
  return 0;
}

int set_layer_mouse(Combo combo) {
  /* Set current layer to mouse ( mouse u/d/l/r, button l/r, scroll u/d ) */
  current_layer_ptr = layer_mouse;
  current_layer_len = layer_mouse_len;
  return 0;
}

/* ============================== device driver ============================== */                  

                                   
void ble_press_key(uint8_t keycode, uint8_t modifier) {
    /* Send a key press over bluetooth connection. */
    uint8_t report_code[6] = { 0 };
    report_code[0] = keycode;
    blehid.keyboardReport(modifier, report_code);
    delay(5); // delay for a bit after report
    blehid.keyRelease();
    //delay(5);
    //delay(5);
}


void ble_press_key(uint8_t keycode) {
    /* Send a key press over bluetooth connection. */
    uint8_t modifier = 0;
    ble_press_key(keycode, modifier);
}


uint8_t get_keyboard_state(int pinmap[8]) {
  /* Return the state of a list of 8 pins as a byte.
   *  Direct port access is faster but idk if it is needed.
   */
  uint8_t result = 0x0;
  for (int i = 0; i < 8; i++) {
    int pin_state = digitalRead(pinmap[i]);
    result = result | (pin_state << i);
  }
  return result;
}


Combo keycode_to_combo(uint8_t keycode, PRESS_TYPE press_type) {
  /* Try to find combo that matches keycode in the current_layer */
  // Serial.println(current_layer_len);
  for (int i=0; i < current_layer_len; i++) {
    // Combo combo = current_layer[i];
    Combo combo = *(current_layer_ptr + i);
    if (combo.keycode == keycode && combo.press_type == press_type) {
      return combo;
    }
  }
  Combo no_key = {255, TAP, HID_KEY_NONE, NULL, do_nothing};
  Serial.println("Failure to match combo, returning KEY_NONE");
  Serial.println(current_layer_len);
  return no_key;
}

void setup() {
  startAdv();
  
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }

  set_layer_primary(layer_primary[0]);
}


bool is_key_pressed = false;
unsigned long press_start_time;
uint8_t active_code = 0;

// log of keyboard state, only becomes active when keys are pressed
#define STATE_LOG_SIZE 100
int state_log[STATE_LOG_SIZE] = {0};
int *state_log_ptr = state_log;


void tick() {
  uint8_t code_in = get_keyboard_state(pins);
  if (code_in < 255) { // 255 means no keys are pressed
    Serial.println("=====================================");
    // Serial.print("State of the pins: ");
    // Serial.println(code_in, BIN);
    Serial.print("Pincode: ");
    Serial.println(code_in);

    if (is_key_pressed == false) {
      delay(press_delay); // wait and reget the pins makes it more accurate
      active_code = get_keyboard_state(pins);
      if (active_code == 255) {
        active_code = code_in;
      }
      Serial.println(code_in);
      Serial.println(active_code);
      press_start_time = millis();
      is_key_pressed = true;
    }
  } else { // all keys released
    if (is_key_pressed == true) {
      unsigned long press_time = millis() - press_start_time; // how long of a keypress
      PRESS_TYPE press_type = TAP;
      if (press_time >= hold_time_ms) { // one second
        press_type = HOLD;
      }
      press_start_time = 0;
      Serial.print("press type detected: ");
      Serial.println(press_type);
      Combo combo = keycode_to_combo(active_code, press_type);
      
      combo.func(combo);
      is_key_pressed = false;
      delay(release_delay);
    }
    blehid.keyRelease();
  }
  delay(poll_delay_ms);
}


void loop() {
  tick();
}
