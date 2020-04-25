#include "index.h"

Napi::ThreadSafeFunction tsf_log;
Napi::ThreadSafeFunction tsf_change_state;

Napi::Boolean change_state(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    Napi::Object js_sc = info[0].As<Napi::Object>();
    lc::StateChange sc = create_StateChange(js_sc);
    if (!lc::enqueue_StateChange(sc))
    {
        lc::js_log("LivingColors exception: error in native code");
        return Napi::Boolean::New(env, false);
    }
    return Napi::Boolean::New(env, true);
}

Napi::Boolean setup(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 2)
    {
        lc::js_log("LivingColors exception: wrong number of arguments");
        return Napi::Boolean::New(env, false);
    }
    if (!info[0].IsFunction() || !info[1].IsFunction())
    {
        lc::js_log("LivingColors exception: argument of wrong type");
        return Napi::Boolean::New(env, false);
    }
    tsf_log = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "JavaScript log callback",
        0,
        1);
    tsf_change_state = Napi::ThreadSafeFunction::New(
        env,
        info[1].As<Napi::Function>(),
        "JavaScript changeState callback",
        0,
        1);
    if (!lc::setup())
    {
        lc::js_log("LivingColors exception: error in native code");
        return Napi::Boolean::New(env, false);
    }
    else
    {
        lc::js_log("LivingColors info: setup successful");
    }
    return Napi::Boolean::New(env, true);
}

Napi::Boolean stop(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    lc::js_log("LivingColors info: stopping...");
    lc::stop();
    tsf_log.Release();
    tsf_change_state.Release();
    return Napi::Boolean::New(env, true);
}

lc::StateChange create_StateChange(Napi::Object &js_sc)
{
    lc::StateChange sc;
    sc.lamp = NapiValue_uint32(js_sc.Get(LC_SC_LAMP));
    sc.command = NapiValue_uint8(js_sc.Get(LC_SC_COMMAND));
    sc.hue = NapiValue_uint8(js_sc.Get(LC_SC_HUE));
    sc.saturation = NapiValue_uint8(js_sc.Get(LC_SC_SATURATION));
    sc.value = NapiValue_uint8(js_sc.Get(LC_SC_VALUE));
    return sc;
}

Napi::Object create_js_StateChange(Napi::Env &env, lc::StateChange &sc)
{
    Napi::Object js_sc = Napi::Object::New(env);
    js_sc.Set(LC_SC_LAMP, Napi::Number::New(env, sc.lamp));
    js_sc.Set(LC_SC_COMMAND, Napi::Number::New(env, sc.command));
    js_sc.Set(LC_SC_HUE, Napi::Number::New(env, sc.hue));
    js_sc.Set(LC_SC_SATURATION, Napi::Number::New(env, sc.saturation));
    js_sc.Set(LC_SC_VALUE, Napi::Number::New(env, sc.value));
    return js_sc;
}

unsigned char NapiValue_uint8(Napi::Value value)
{
    return (unsigned char)NapiValue_uint32(value);
}

uint32_t NapiValue_uint32(Napi::Value value)
{
    return value.As<Napi::Number>().Uint32Value();
}

void js_cb_log(Napi::Env env, Napi::Function js_log, std::string *msg)
{
    js_log.Call({Napi::String::New(env, *msg)});
    delete msg;
}

void js_cb_change_state(Napi::Env env, Napi::Function js_change_state, lc::StateChange *sc)
{
    js_change_state.Call({create_js_StateChange(env, *sc)});
    delete sc;
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "changeState"), Napi::Function::New(env, change_state));
    exports.Set(Napi::String::New(env, "setup"), Napi::Function::New(env, setup));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, stop));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)