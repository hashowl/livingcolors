#ifndef INDEX_H
#define INDEX_H

#ifndef NAPI_VERSION
#define NAPI_VERSION 5
#endif

#define LC_SC_LAMP "lamp"
#define LC_SC_COMMAND "command"
#define LC_SC_HUE "hue"
#define LC_SC_SATURATION "saturation"
#define LC_SC_VALUE "value"

#include <napi.h>
#include <string>
#include "livingcolors.h"

Napi::Boolean cmd(const Napi::CallbackInfo &);
Napi::Boolean setup(const Napi::CallbackInfo &);
Napi::Boolean stop(const Napi::CallbackInfo &);

lc::StateChange create_StateChange(Napi::Object &);
Napi::Object create_js_StateChange(Napi::Env &, lc::StateChange &);

unsigned char NapiValue_uint8(Napi::Value);
uint32_t NapiValue_uint32(Napi::Value);

void js_cb_log(Napi::Env, Napi::Function, std::string *);
void js_cb_ack(Napi::Env, Napi::Function, lc::StateChange *);

Napi::Object Init(Napi::Env, Napi::Object);

#endif