#ifndef INDEX_H
#define INDEX_H

#ifndef NAPI_VERSION
#define NAPI_VERSION 5
#endif

#define SC_LAMP "lamp"
#define SC_COMMAND "command"
#define SC_HUE "hue"
#define SC_SATURATION "saturation"
#define SC_VALUE "value"

#include <napi.h>
#include <string>

#include "livingcolors.h"

extern Napi::ThreadSafeFunction tsf_log;
extern Napi::ThreadSafeFunction tsf_changeState;

Napi::Number changeState(const Napi::CallbackInfo &);
Napi::Number setup(const Napi::CallbackInfo &);
Napi::Number stop(const Napi::CallbackInfo &);

lc::StateChange createStateChange(const Napi::Object);
Napi::Object create_js_StateChange(const Napi::Env, const lc::StateChange &);

unsigned char NapiValue_uint8(const Napi::Value);
uint32_t NapiValue_uint32(const Napi::Value);

void js_cb_log(Napi::Env, Napi::Function, std::string *);
void js_cb_changeState(Napi::Env, Napi::Function, lc::StateChange *);

Napi::Object Init(Napi::Env, Napi::Object);

#endif