#include "index.h"

Napi::ThreadSafeFunction tsf_log;
Napi::ThreadSafeFunction tsf_changeState;

Napi::Number changeState(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    Napi::Object js_sc = info[0].As<Napi::Object>();
    lc::StateChange sc = createStateChange(js_sc);
    int res = lc::enqueueStateChange(sc);
    if (res)
    {
        lc::js_log("LivingColors.changeState() failed: internal error");
    }
    return Napi::Number::New(env, res);
}

Napi::Number setup(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 2)
    {
        lc::js_log("LivingColors.setup() failed: wrong number of arguments");
        return Napi::Number::New(env, 1);
    }
    if (!info[0].IsFunction() || !info[1].IsFunction())
    {
        lc::js_log("LivingColors.setup() failed: argument of wrong type");
        return Napi::Number::New(env, 1);
    }
    tsf_log = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "JavaScript log callback",
        0,
        1);
    tsf_changeState = Napi::ThreadSafeFunction::New(
        env,
        info[1].As<Napi::Function>(),
        "JavaScript changeState callback",
        0,
        1);
    int res = lc::setup();
    if (res)
    {
        lc::js_log("LivingColors.setup() failed: internal error");
    }
    else
    {
        lc::js_log("LivingColors.setup() successful");
    }
    return Napi::Number::New(env, res);
}

Napi::Number stop(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    return Napi::Number::New(env, 0);
}

lc::StateChange createStateChange(const Napi::Object js_sc)
{
    lc::StateChange sc;
    sc.lamp = NapiValue_uint32(js_sc.Get(SC_LAMP));
    sc.command = NapiValue_uint8(js_sc.Get(SC_COMMAND));
    sc.hue = NapiValue_uint8(js_sc.Get(SC_HUE));
    sc.saturation = NapiValue_uint8(js_sc.Get(SC_SATURATION));
    sc.value = NapiValue_uint8(js_sc.Get(SC_VALUE));
    return sc;
}

Napi::Object create_js_StateChange(const Napi::Env env, const lc::StateChange &sc)
{
    Napi::Object js_sc = Napi::Object::New(env);
    js_sc.Set(SC_LAMP, Napi::Number::New(env, sc.lamp));
    js_sc.Set(SC_COMMAND, Napi::Number::New(env, sc.command));
    js_sc.Set(SC_HUE, Napi::Number::New(env, sc.hue));
    js_sc.Set(SC_SATURATION, Napi::Number::New(env, sc.saturation));
    js_sc.Set(SC_VALUE, Napi::Number::New(env, sc.value));
    return js_sc;
}

unsigned char NapiValue_uint8(const Napi::Value value)
{
    return (unsigned char)NapiValue_uint32(value);
}

uint32_t NapiValue_uint32(const Napi::Value value)
{
    return value.As<Napi::Number>().Uint32Value();
}

void js_cb_log(Napi::Env env, Napi::Function js_log, std::string *msg)
{
    js_log.Call({Napi::String::New(env, *msg)});
    delete msg;
}

void js_cb_changeState(Napi::Env env, Napi::Function js_changeState, lc::StateChange *sc)
{
    js_changeState.Call({create_js_StateChange(env, *sc)});
    delete sc;
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "changeState"), Napi::Function::New(env, changeState));
    exports.Set(Napi::String::New(env, "setup"), Napi::Function::New(env, setup));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, stop));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)