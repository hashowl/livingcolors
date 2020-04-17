#include <napi.h>

using namespace std;

Napi::Number setup(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, 0);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("setup", Napi::Function::New(env, setup));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)