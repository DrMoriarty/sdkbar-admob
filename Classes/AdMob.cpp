#include "AdMob.hpp"
#include "scripting/js-bindings/manual/cocos2d_specifics.hpp"
#include "scripting/js-bindings/manual/js_manual_conversions.h"
#include "platform/android/jni/JniHelper.h"
#include <jni.h>
#include <sstream>
#include "base/CCDirector.h"
#include "base/CCScheduler.h"
#include "utils/PluginUtils.h"
#include "firebase/app.h"
#include "firebase/admob.h"
#include "firebase/admob/banner_view.h"
#include "firebase/admob/interstitial_ad.h"
#include "firebase/remote_config.h"

static std::string ApplicationId;
static std::vector<std::string> testDeviceIds;
static const char* testingDevices[100];
static firebase::admob::AdRequest my_ad_request = {};
static firebase::admob::BannerView *sharedBannerView = NULL;
static firebase::admob::InterstitialAd *sharedInterstitialAd = NULL;
static bool rewarded_inited = false;

static void printLog(const char* str) {
    CCLOG("%s", str);
}

firebase::admob::AdParent getAdParent() {
    // Returns the Android Activity.
    return cocos2d::JniHelper::getActivity();
}

///////////////////////////////////////
//
//  Plugin Init
//
///////////////////////////////////////

static bool jsb_admob_init(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_init");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        bool ok = true;
        std::string advertisingId;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &advertisingId);

        printLog("[AdMob] Init plugin");
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
        // Initialize Firebase for Android.
        firebase::App* app = firebase::App::Create(firebase::AppOptions(), cocos2d::JniHelper::getEnv(), cocos2d::JniHelper::getActivity());
        // Initialize AdMob.
        firebase::admob::Initialize(*app, advertisingId.c_str());
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
        // Initialize Firebase for iOS.
        firebase::App* app = firebase::App::Create(firebase::AppOptions());
        // Initialize AdMob.
        firebase::admob::Initialize(*app, advertisingId.c_str());
#endif
        if(firebase::remote_config::Initialize(*app) == firebase::kInitResultSuccess) {
            if(firebase::remote_config::ActivateFetched()) {
                printLog("Firebase: activate fetched config");
            }
            firebase::remote_config::Fetch();
        }
        ApplicationId = advertisingId;
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_launch_test_suite(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_launch_test_suite");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        if(ApplicationId.size() > 0) {

            cocos2d::JniMethodInfo methodInfo;
            if (! cocos2d::JniHelper::getStaticMethodInfo(methodInfo, "com/google/android/ads/mediationtestsuite/MediationTestSuite", "launch", "(Landroid/content/Context;Ljava/lang/String;)V")) {
                rec.rval().set(JSVAL_FALSE);
                return false;
            }
            jstring str = methodInfo.env->NewStringUTF(ApplicationId.c_str());
            methodInfo.env->CallStaticVoidMethod(methodInfo.classID, methodInfo.methodID, cocos2d::JniHelper::getActivity(), str);
            methodInfo.env->DeleteLocalRef(str);
            methodInfo.env->DeleteLocalRef(methodInfo.classID);

            rec.rval().set(JSVAL_TRUE);
        } else {
            rec.rval().set(JSVAL_FALSE);
        }
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_add_test_device(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_add_test_device");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        // device id
        bool ok = true;
        std::string deviceId;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &deviceId);
        testDeviceIds.push_back(deviceId);

        my_ad_request.test_device_id_count = testDeviceIds.size();
        for(int i=0; i<testDeviceIds.size(); i++) {
            testingDevices[i] = testDeviceIds[i].c_str();
            printLog(testDeviceIds[i].c_str());
        }
        my_ad_request.test_device_ids = testingDevices;
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  Remote Config
//
///////////////////////////////////////

static bool jsb_admob_get_boolean(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_get_boolean");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        // key
        bool ok = true;
        std::string key;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &key);
        if(firebase::remote_config::GetBoolean(key.c_str())) {
            rec.rval().set(JSVAL_TRUE);
        } else {
            rec.rval().set(JSVAL_FALSE);
        }
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}


static bool jsb_admob_get_integer(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_get_integer");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        // key
        bool ok = true;
        std::string key;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &key);
        int64_t value = firebase::remote_config::GetLong(key.c_str());
        rec.rval().set(JS::Int32Value(value));
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_get_double(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_get_double");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        // key
        bool ok = true;
        std::string key;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &key);
        double value = firebase::remote_config::GetDouble(key.c_str());
        rec.rval().set(JS::DoubleValue(value));
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_get_string(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_get_string");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 1) {
        // key
        bool ok = true;
        std::string key;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &key);
        std::string value = firebase::remote_config::GetString(key.c_str());
        rec.rval().set(std_string_to_jsval(cx, value));
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  Banner
//
///////////////////////////////////////

typedef struct BannerSettings {
    firebase::admob::BannerView *bannerView;
    int callbackId;
    BannerSettings(firebase::admob::BannerView *ad, int cbId) {
        bannerView = ad;
        callbackId = cbId;
    };
} BannerSettings;

/*
static void BannerShowCallback(const firebase::Future<void>& future, void* user_data) {
    BannerSettings *settings = static_cast<BannerSettings*>(user_data);
    CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
    JS::AutoValueVector valArr(cb->cx);
    if (future.error() == firebase::admob::kAdMobErrorNone) {
        printLog("Banner show complete");
        valArr.append(JSVAL_TRUE);
    } else {
        printLog("Banner show error");
        valArr.append(JSVAL_FALSE);
    }
    JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
    cb->call(funcArgs);
    delete settings;
    delete cb;
}
*/

static void BannerLoadCallback(const firebase::Future<void>& future, void* user_data) {
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([future, user_data] {
            BannerSettings *settings = static_cast<BannerSettings*>(user_data);
            CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
            JSAutoRequest rq(cb->cx);
            JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
            JS::AutoValueVector valArr(cb->cx);
            if (future.error() == firebase::admob::kAdMobErrorNone) {
                printLog("Banner load complete");
                valArr.append(JSVAL_TRUE);
            } else {
                printLog("Banner load error");
                valArr.append(JSVAL_FALSE);
            }
            JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
            cb->call(funcArgs);
            delete settings;
            delete cb;
        });
}

static void BannerInitCallback(const firebase::Future<void>& future, void* user_data) {
    BannerSettings *settings = static_cast<BannerSettings*>(user_data);
    if (future.error() == firebase::admob::kAdMobErrorNone) {
        printLog("Banner init complete");
        settings->bannerView->LoadAd(my_ad_request);
        settings->bannerView->LoadAdLastResult().OnCompletion(BannerLoadCallback, settings);
    } else {
        printLog("Banner init error");
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([settings] {
                CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(JSVAL_FALSE);
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
                delete settings;
                delete cb;
            });
    }
}

static void BannerHideCallback(const firebase::Future<void>& future, void* user_data) {
    firebase::admob::BannerView *bannerView = static_cast<firebase::admob::BannerView*>(user_data);
    if (future.error() == firebase::admob::kAdMobErrorNone) {
        printLog("Banner hide complete");
        bannerView->Destroy();
    } else {
        printLog("Banner hide error");
    }
}

class MyBannerViewListener : public firebase::admob::BannerView::Listener {
    CallbackFrame *cb;
public:
    MyBannerViewListener(int callbackId) {
        cb = CallbackFrame::getById(callbackId);
    }

    void OnPresentationStateChanged(firebase::admob::BannerView* banner_view, firebase::admob::BannerView::PresentationState state) override {
        // This method gets called when the banner view's presentation
        // state changes.
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([this, state] {
                printLog("[AdMob] Banner state changed");
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(int32_to_jsval(cb->cx, state));
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
            });
    }

    void OnBoundingBoxChanged(firebase::admob::BannerView* banner_view, firebase::admob::BoundingBox box) override {
        // This method gets called when the banner view's bounding box
        // changes.
        printLog("[AdMob] Banner size changed");
    }

    ~MyBannerViewListener() {
        delete cb;
    }
};

static bool jsb_admob_load_banner(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_load_banner");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 3) {
        // banner id, callback, this
        bool ok = true;
        std::string bannerId;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &bannerId);
        CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(2), args.get(1));

        firebase::admob::AdSize ad_size;
        ad_size.ad_size_type = firebase::admob::kAdSizeStandard;
        ad_size.width = 320;
        ad_size.height = 50;
        sharedBannerView = new firebase::admob::BannerView();
        BannerSettings *settings = new BannerSettings(sharedBannerView, cb->callbackId);
        sharedBannerView->Initialize(getAdParent(), bannerId.c_str(), ad_size);
        sharedBannerView->InitializeLastResult().OnCompletion(BannerInitCallback, settings);
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_is_banner_loaded(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_is_banner_loaded");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        if (sharedBannerView != NULL &&
            sharedBannerView->LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
            sharedBannerView->LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_show_banner(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_show_banner");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 2) {
        if(sharedBannerView != NULL &&
           sharedBannerView->LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
           sharedBannerView->LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {
            CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(1), args.get(0));
            //BannerSettings *settings = new BannerSettings(sharedBannerView, cb->callbackId);
            sharedBannerView->SetListener(new MyBannerViewListener(cb->callbackId));
            sharedBannerView->Show();
            //sharedBannerView->ShowLastResult().OnCompletion(BannerShowCallback, settings);
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_close_banner(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_close_banner");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        if(sharedBannerView != NULL &&
           sharedBannerView->ShowLastResult().status() == firebase::kFutureStatusComplete &&
           sharedBannerView->ShowLastResult().error() == firebase::admob::kAdMobErrorNone) {
            sharedBannerView->Hide();
            sharedBannerView->HideLastResult().OnCompletion(BannerHideCallback, sharedBannerView);
            sharedBannerView = NULL;
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  Interstitial Ad
//
///////////////////////////////////////

typedef struct InterstitialSettings {
    firebase::admob::InterstitialAd *interstitial_ad;
    int callbackId;
    InterstitialSettings(firebase::admob::InterstitialAd *ad, int cbId) {
        interstitial_ad = ad;
        callbackId = cbId;
    };
} InterstitialSettings;

static void InterstitialLoadCallback(const firebase::Future<void>& future, void* user_data) {
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([future, user_data] {
            InterstitialSettings *settings = static_cast<InterstitialSettings*>(user_data);
            CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
            JSAutoRequest rq(cb->cx);
            JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
            JS::AutoValueVector valArr(cb->cx);
            if (future.error() == firebase::admob::kAdMobErrorNone) {
                printLog("Interstitial load complete");
                valArr.append(JSVAL_TRUE);
            } else {
                printLog("Interstitial load error");
                valArr.append(JSVAL_FALSE);
            }
            JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
            cb->call(funcArgs);
            delete settings;
            delete cb;
        });
}

static void InterstitialInitCallback(const firebase::Future<void>& future, void* user_data) {
    InterstitialSettings *settings = static_cast<InterstitialSettings*>(user_data);
    if (future.error() == firebase::admob::kAdMobErrorNone) {
        settings->interstitial_ad->LoadAd(my_ad_request);
        settings->interstitial_ad->LoadAdLastResult().OnCompletion(InterstitialLoadCallback, settings);
        printLog("Interstitial init complete");
    } else {
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([settings] {
                printLog("Interstitial init error");
                CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(JSVAL_FALSE);
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
                delete settings;
                delete cb;
            });
    }
}

class MyInterstitialAdListener: public firebase::admob::InterstitialAd::Listener {
    CallbackFrame *cb;
public:
    MyInterstitialAdListener(int callbackId) {
        cb = CallbackFrame::getById(callbackId);
    };

    void OnPresentationStateChanged(firebase::admob::InterstitialAd* interstitialAd, firebase::admob::InterstitialAd::PresentationState state) override {
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([this, state] {
                printLog("[AdMob] InterstitialAd state changed");
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(int32_to_jsval(cb->cx, state));
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
            });
    }
    
    ~MyInterstitialAdListener() {
        delete cb;
    }
};

static bool jsb_admob_load_interstitial(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_load_interstitial");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 3) {
        // banner id, callback, this
        CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(2), args.get(1));
        bool ok = true;
        std::string bannerId;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &bannerId);

        sharedInterstitialAd = new firebase::admob::InterstitialAd();
        InterstitialSettings *settings = new InterstitialSettings(sharedInterstitialAd, cb->callbackId);
        sharedInterstitialAd->Initialize(getAdParent(), bannerId.c_str());
        sharedInterstitialAd->InitializeLastResult().OnCompletion(InterstitialInitCallback, settings);
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_is_interstitial_loaded(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_is_interstitial_loaded");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        if (sharedInterstitialAd != NULL &&
            sharedInterstitialAd->LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
            sharedInterstitialAd->LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_show_interstitial(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_show_interstitial");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 2) {
        if (sharedInterstitialAd != NULL &&
            sharedInterstitialAd->LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
            sharedInterstitialAd->LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {
            CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(1), args.get(0));
            sharedInterstitialAd->SetListener(new MyInterstitialAdListener(cb->callbackId));
            sharedInterstitialAd->Show();
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  Rewarded Ad
//
///////////////////////////////////////

typedef struct RewardedSettings {
    std::string adId;
    int callbackId;
    RewardedSettings(std::string& _adId, int _cbId) {
        adId = _adId;
        callbackId = _cbId;
    };
} RewardedSettings;

static void RewardedLoadedCallback(const firebase::Future<void>& future, void* user_data) {
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([future, user_data] {
            RewardedSettings *settings = static_cast<RewardedSettings*>(user_data);
            CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
            JSAutoRequest rq(cb->cx);
            JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
            JS::AutoValueVector valArr(cb->cx);
            if (future.error() == firebase::admob::kAdMobErrorNone) {
                //firebase::admob::rewarded_video::Show(getAdParent());
                printLog("Rewarded load complete");
                valArr.append(JSVAL_TRUE);
            } else {
                printLog("Rewarded load error");
                valArr.append(JSVAL_FALSE);
            }
            JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
            cb->call(funcArgs);
            delete settings;
            delete cb;
        });
}

static void RewardedInitCallback(const firebase::Future<void>& future, void* user_data) {
    RewardedSettings *settings = static_cast<RewardedSettings*>(user_data);
    if (future.error() == firebase::admob::kAdMobErrorNone) {
        rewarded_inited = true;
        firebase::admob::rewarded_video::LoadAd(settings->adId.c_str(), my_ad_request);
        firebase::admob::rewarded_video::LoadAdLastResult().OnCompletion(RewardedLoadedCallback, settings);
        printLog("Rewarded init complete");
    } else {
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([settings] {
                printLog("Rewarded init error");
                CallbackFrame *cb = CallbackFrame::getById(settings->callbackId);
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(JSVAL_FALSE);
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
                delete settings;
                delete cb;
            });
    }
}

class MyRewardedVideoListener: public firebase::admob::rewarded_video::Listener {
    CallbackFrame *cb;
public:
    MyRewardedVideoListener(int callbackId) {
        cb = CallbackFrame::getById(callbackId);
    };

    void OnRewarded(firebase::admob::rewarded_video::RewardItem item) override {
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([this] {
                printLog("[AdMob] On reward item");
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(int32_to_jsval(cb->cx, 3));
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
            });
    }

    void OnPresentationStateChanged(firebase::admob::rewarded_video::PresentationState state) override {
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([this, state] {
                printLog("[AdMob] InterstitialAd state changed");
                JSAutoRequest rq(cb->cx);
                JSAutoCompartment ac(cb->cx, cb->_ctxObject.ref());
                JS::AutoValueVector valArr(cb->cx);
                valArr.append(int32_to_jsval(cb->cx, state));
                JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                cb->call(funcArgs);
            });
    }
    
    ~MyRewardedVideoListener() {
        delete cb;
    }
};

static bool jsb_admob_load_rewarded(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_load_rewarded");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 3) {
        // banner id, callback, this
        CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(2), args.get(1));
        bool ok = true;
        std::string bannerId;
        JS::RootedValue arg0Val(cx, args.get(0));
        ok &= jsval_to_std_string(cx, arg0Val, &bannerId);
        RewardedSettings *settings = new RewardedSettings(bannerId, cb->callbackId);
        if(!rewarded_inited) {
            firebase::admob::rewarded_video::Initialize();
            firebase::admob::rewarded_video::InitializeLastResult().OnCompletion(RewardedInitCallback, settings);
        } else {
            firebase::admob::rewarded_video::LoadAd(settings->adId.c_str(), my_ad_request);
            firebase::admob::rewarded_video::LoadAdLastResult().OnCompletion(RewardedLoadedCallback, settings);
        }
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_is_rewarded_loaded(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_is_rewarded_loaded");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        if (rewarded_inited &&
            firebase::admob::rewarded_video::LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
            firebase::admob::rewarded_video::LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {
            rec.rval().set(JSVAL_TRUE);
            printLog("Admob: rewarded is loaded!");
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            printLog("Admob: rewarded not loaded");
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_admob_show_rewarded(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_admob_show_rewarded");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 2) {
        if (firebase::admob::rewarded_video::LoadAdLastResult().status() == firebase::kFutureStatusComplete &&
            firebase::admob::rewarded_video::LoadAdLastResult().error() == firebase::admob::kAdMobErrorNone) {

            CallbackFrame *cb = new CallbackFrame(cx, obj, args.get(1), args.get(0));
            firebase::admob::rewarded_video::SetListener(new MyRewardedVideoListener(cb->callbackId));
            firebase::admob::rewarded_video::Show(getAdParent());
            rec.rval().set(JSVAL_TRUE);
            printLog("Admob: rewarded started");
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            printLog("Admob: rewarded not started");
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  Register JS API
//
///////////////////////////////////////

void register_all_admob_framework(JSContext* cx, JS::HandleObject obj) {
    printLog("[AdMob] register js interface");
    JS::RootedObject ns(cx);
    get_or_create_js_obj(cx, obj, "admob", &ns);

    JS_DefineFunction(cx, ns, "init", jsb_admob_init, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "launch_test_suite", jsb_admob_launch_test_suite, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "add_test_device", jsb_admob_add_test_device, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    JS_DefineFunction(cx, ns, "get_boolean", jsb_admob_get_boolean, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "get_integer", jsb_admob_get_integer, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "get_double", jsb_admob_get_double, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "get_string", jsb_admob_get_string, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    JS_DefineFunction(cx, ns, "load_banner", jsb_admob_load_banner, 3, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "is_banner_loaded", jsb_admob_is_banner_loaded, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "show_banner", jsb_admob_show_banner, 2, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "close_banner", jsb_admob_close_banner, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    JS_DefineFunction(cx, ns, "load_interstitial", jsb_admob_load_interstitial, 3, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "is_interstitial_loaded", jsb_admob_is_interstitial_loaded, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "show_interstitial", jsb_admob_show_interstitial, 2, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    JS_DefineFunction(cx, ns, "load_rewarded", jsb_admob_load_rewarded, 3, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "is_rewarded_loaded", jsb_admob_is_rewarded_loaded, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "show_rewarded", jsb_admob_show_rewarded, 2, JSPROP_ENUMERATE | JSPROP_PERMANENT);

}
