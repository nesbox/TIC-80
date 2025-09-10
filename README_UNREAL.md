# Including Tic-80 in an Unreal Engine Plugin

## Build the libraries + set them up in an unreal engine plugin

1. Had to update CMAKE in order to support building static libraries so now run these two commands:

```bash
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIB=ON -DBUILD_STATIC=true -DBUILD_WITH_RUBY=false -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL" ..
cmake --build . --config Release --parallel
```

2. QuickJS likely won't compile with windows MSVCC so some changes need to be made (looks related to this pull request). Basically NAN seems not supported as a constant expression so A) we need to define it and B) we need to move some of the array initialization code that depends on macros that use NAN to runtime code:

```diff
diff --git a/quickjs.c b/quickjs.c
index 681e998..135d98e 100644
--- a/quickjs.c
+++ b/quickjs.c
@@ -41107,7 +41107,8 @@ static const JSCFunctionListEntry js_number_funcs[] = {
     JS_CFUNC_DEF("isSafeInteger", 1, js_number_isSafeInteger ),
     JS_PROP_DOUBLE_DEF("MAX_VALUE", 1.7976931348623157e+308, 0 ),
     JS_PROP_DOUBLE_DEF("MIN_VALUE", 5e-324, 0 ),
-    JS_PROP_DOUBLE_DEF("NaN", NAN, 0 ),
+    // JS_PROP_DOUBLE_DEF("NaN", NAN, 0 ),
+    JS_PROP_U2D_DEF("NaN", 0x7FF8ull<<48, 0 ), // workaround for msvc
     JS_PROP_DOUBLE_DEF("NEGATIVE_INFINITY", -INFINITY, 0 ),
     JS_PROP_DOUBLE_DEF("POSITIVE_INFINITY", INFINITY, 0 ),
     JS_PROP_DOUBLE_DEF("EPSILON", 2.220446049250313e-16, 0 ), /* ES6 */
@@ -43367,6 +43368,66 @@ static JSValue js_math_random(JSContext *ctx, JSValueConst this_val,
     return __JS_NewFloat64(ctx, u.d - 1.0);
 }
 
+#ifdef _MSC_VER
+/* MSVC doesn't allow function pointers in static initializers */
+static JSCFunctionListEntry js_math_funcs[39];
+
+static void init_js_math_funcs(void) {
+    static int initialized = 0;
+    if (initialized) return;
+    
+    JSCFunctionListEntry funcs[] = {
+        JS_CFUNC_MAGIC_DEF("min", 2, js_math_min_max, 0 ),
+        JS_CFUNC_MAGIC_DEF("max", 2, js_math_min_max, 1 ),
+        JS_CFUNC_SPECIAL_DEF("abs", 1, f_f, fabs ),
+        JS_CFUNC_SPECIAL_DEF("floor", 1, f_f, floor ),
+        JS_CFUNC_SPECIAL_DEF("ceil", 1, f_f, ceil ),
+        JS_CFUNC_SPECIAL_DEF("round", 1, f_f, js_math_round ),
+        JS_CFUNC_SPECIAL_DEF("sqrt", 1, f_f, sqrt ),
+
+        JS_CFUNC_SPECIAL_DEF("acos", 1, f_f, acos ),
+        JS_CFUNC_SPECIAL_DEF("asin", 1, f_f, asin ),
+        JS_CFUNC_SPECIAL_DEF("atan", 1, f_f, atan ),
+        JS_CFUNC_SPECIAL_DEF("atan2", 2, f_f_f, atan2 ),
+        JS_CFUNC_SPECIAL_DEF("cos", 1, f_f, cos ),
+        JS_CFUNC_SPECIAL_DEF("exp", 1, f_f, exp ),
+        JS_CFUNC_SPECIAL_DEF("log", 1, f_f, log ),
+        JS_CFUNC_SPECIAL_DEF("pow", 2, f_f_f, js_pow ),
+        JS_CFUNC_SPECIAL_DEF("sin", 1, f_f, sin ),
+        JS_CFUNC_SPECIAL_DEF("tan", 1, f_f, tan ),
+        /* ES6 */
+        JS_CFUNC_SPECIAL_DEF("trunc", 1, f_f, trunc ),
+        JS_CFUNC_SPECIAL_DEF("sign", 1, f_f, js_math_sign ),
+        JS_CFUNC_SPECIAL_DEF("cosh", 1, f_f, cosh ),
+        JS_CFUNC_SPECIAL_DEF("sinh", 1, f_f, sinh ),
+        JS_CFUNC_SPECIAL_DEF("tanh", 1, f_f, tanh ),
+        JS_CFUNC_SPECIAL_DEF("acosh", 1, f_f, acosh ),
+        JS_CFUNC_SPECIAL_DEF("asinh", 1, f_f, asinh ),
+        JS_CFUNC_SPECIAL_DEF("atanh", 1, f_f, atanh ),
+        JS_CFUNC_SPECIAL_DEF("expm1", 1, f_f, expm1 ),
+        JS_CFUNC_SPECIAL_DEF("log1p", 1, f_f, log1p ),
+        JS_CFUNC_SPECIAL_DEF("log2", 1, f_f, log2 ),
+        JS_CFUNC_SPECIAL_DEF("log10", 1, f_f, log10 ),
+        JS_CFUNC_SPECIAL_DEF("cbrt", 1, f_f, cbrt ),
+        JS_CFUNC_DEF("hypot", 2, js_math_hypot ),
+        JS_CFUNC_DEF("random", 0, js_math_random ),
+        JS_CFUNC_SPECIAL_DEF("fround", 1, f_f, js_math_fround ),
+        JS_CFUNC_DEF("imul", 2, js_math_imul ),
+        JS_CFUNC_DEF("clz32", 1, js_math_clz32 ),
+        JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Math", JS_PROP_CONFIGURABLE ),
+        JS_PROP_DOUBLE_DEF("E", 2.718281828459045, 0 ),
+        JS_PROP_DOUBLE_DEF("LN10", 2.302585092994046, 0 ),
+        JS_PROP_DOUBLE_DEF("LN2", 0.6931471805599453, 0 ),
+        JS_PROP_DOUBLE_DEF("LOG2E", 1.4426950408889634, 0 ),
+        JS_PROP_DOUBLE_DEF("LOG10E", 0.4342944819032518, 0 ),
+        JS_PROP_DOUBLE_DEF("PI", 3.141592653589793, 0 ),
+        JS_PROP_DOUBLE_DEF("SQRT1_2", 0.7071067811865476, 0 ),
+        JS_PROP_DOUBLE_DEF("SQRT2", 1.4142135623730951, 0 ),
+    };
+    memcpy(js_math_funcs, funcs, sizeof(funcs));
+    initialized = 1;
+}
+#else
 static const JSCFunctionListEntry js_math_funcs[] = {
     JS_CFUNC_MAGIC_DEF("min", 2, js_math_min_max, 0 ),
     JS_CFUNC_MAGIC_DEF("max", 2, js_math_min_max, 1 ),
@@ -43415,6 +43476,7 @@ static const JSCFunctionListEntry js_math_funcs[] = {
     JS_PROP_DOUBLE_DEF("SQRT1_2", 0.7071067811865476, 0 ),
     JS_PROP_DOUBLE_DEF("SQRT2", 1.4142135623730951, 0 ),
 };
+#endif
 
 static const JSCFunctionListEntry js_math_obj[] = {
     JS_OBJECT_DEF("Math", js_math_funcs, countof(js_math_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
@@ -49712,7 +49774,8 @@ static const JSCFunctionListEntry js_global_funcs[] = {
     JS_CFUNC_DEF("escape", 1, js_global_escape ),
     JS_CFUNC_DEF("unescape", 1, js_global_unescape ),
     JS_PROP_DOUBLE_DEF("Infinity", 1.0 / 0.0, 0 ),
-    JS_PROP_DOUBLE_DEF("NaN", NAN, 0 ),
+    // JS_PROP_DOUBLE_DEF("NaN", NAN, 0 ),
+    JS_PROP_U2D_DEF("NaN", 0x7FF8ull<<48, 0 ), // workaround for msvc
     JS_PROP_UNDEFINED_DEF("undefined", 0 ),
     JS_PROP_STRING_DEF("[Symbol.toStringTag]", "global", JS_PROP_CONFIGURABLE ),
 };
@@ -51279,6 +51342,9 @@ void JS_AddIntrinsicBaseObjects(JSContext *ctx)
 
     /* Math: create as autoinit object */
     js_random_init(ctx);
+#ifdef _MSC_VER
+    init_js_math_funcs();
+#endif
     JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_math_obj, countof(js_math_obj));
 
     /* ES6 Reflect: create as autoinit object */
diff --git a/quickjs.h b/quickjs.h
index d907ecb..9e056fe 100644
--- a/quickjs.h
+++ b/quickjs.h
@@ -1052,6 +1052,7 @@ typedef struct JSCFunctionListEntry {
         const char *str;
         int32_t i32;
         int64_t i64;
+        uint64_t u64;
         double f64;
     } u;
 } JSCFunctionListEntry;
@@ -1078,6 +1079,7 @@ typedef struct JSCFunctionListEntry {
 #define JS_PROP_INT32_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT32, 0, .u = { .i32 = val } }
 #define JS_PROP_INT64_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT64, 0, .u = { .i64 = val } }
 #define JS_PROP_DOUBLE_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_DOUBLE, 0, .u = { .f64 = val } }
+#define JS_PROP_U2D_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_DOUBLE, 0, { .u64 = val } }
 #define JS_PROP_UNDEFINED_DEF(name, prop_flags) { name, prop_flags, JS_DEF_PROP_UNDEFINED, 0, .u = { .i32 = 0 } }
 #define JS_OBJECT_DEF(name, tab, len, prop_flags) { name, prop_flags, JS_DEF_OBJECT, 0, .u = { .prop_list = { tab, len } } }
 #define JS_ALIAS_DEF(name, from) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_ALIAS, 0, .u = { .alias = { from, -1 } } }
```

3. Copy the `*.lib` files from `build/lib/` to your plugin third party directory (`<Plugin>/Source/Thirdparty/Tic80/lib`)
4. Copy the `*.h` files from `include/` to your plugin third party include directory (`<Plugin>/Source/Thirdparty/Tic80/include`)
5. Set up an unreal engine plugin with the following `Build.cs` file setup:

```c#
using System.IO;
using UnrealBuildTool;

public class UnrealTic80Plugin : ModuleRules
{
	public UnrealTic80Plugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string Tic80Path = Path.Combine(ModuleDirectory, "..", "Tic80");
		
		PublicIncludePaths.AddRange( new string[] {"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/include"});
		
		PublicAdditionalLibraries.AddRange(new string[]
		{
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/SDL2main.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/argparse.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/blipbuf.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/dlfcn.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/fennel.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/giflib.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/janet.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/js.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/lpeg.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/lua.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/luaapi.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/moon.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/naett.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/png.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/pocketpy.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/python.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/quickjs.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/scheme.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/sdlgpu.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/squirrel.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/tic80core.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/tic80studio.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/wasm.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/wave_writer.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/wren.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/zip.lib",
			"D:/lyra-starter-game/LyraStarterGame/Plugins/UnrealTic80Plugin/Source/Thirdparty/Tic80/lib/zlib.lib"
		});
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"RHI",
			"AudioMixer"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"SlateCore",
			"Projects"
		});
		
		PublicDefinitions.Add("TIC_RUNTIME_STATIC");
	}
}
```

## Develop unreal engine plugin code

```c++
THIRD_PARTY_INCLUDES_START
#include "tic80.h"
THIRD_PARTY_INCLUDES_END

// Create TIC-80 instance with 44.1kHz sample rate and RGBA8888 format
Tic80Core = tic80_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
Tic80Core->callback.trace = &UTic80Instance::OnTrace; // trace callback
Tic80Core->callback.error = &UTic80Instance::OnError; // error callback
Tic80Core->callback.exit = &UTic80Instance::OnExit; // exot callback

// load a cartridge
const FString FilePath = "...";
TArray<uint8> CartridgeData;
FFileHelper::LoadFileToArray(CartridgeData, *FilePath);
tic80_load(Tic80Core, (void*)CartridgeData.GetData(), CartridgeData.Num());

// in an OnTick function
tic80_tick(Tic80Core, CurrentInput, &UTic80Instance::GetCounter, &UTic80Instance::GetFrequency);
tic80_sound(Tic80Core);

// get the sound out into a buffer
TArray<int16>& OutAudioData;
OutAudioData.SetNum(Tic80Core->samples.count);
FMemory::Memcpy(OutAudioData.GetData(), Tic80Core->samples.buffer, Tic80Core->samples.count * sizeof(int16));

// Convert TIC-80 screen buffer to Unreal pixel format
TArray<FColor> PixelBuffer;
PixelBuffer.SetNum(TIC80_FULLWIDTH * TIC80_FULLHEIGHT);
const uint32* Src = Tic80Core->screen;
for (int32 i = 0; i < TIC80_FULLWIDTH * TIC80_FULLHEIGHT; ++i)
{
    uint32 Pixel = Src[i];
    // Convert RGBA to BGRA for Unreal
    uint8 R = (Pixel & 0x000000FF) >> 0;   // or just: Pixel & 0xFF
    uint8 G = (Pixel & 0x0000FF00) >> 8;
    uint8 B = (Pixel & 0x00FF0000) >> 16;
    uint8 A = (Pixel & 0xFF000000) >> 24;
    PixelBuffer[i] = FColor(R, G, B, A);
}

// when ready to destroy:
tic80_delete(Tic80Core);
```

### Play Audio using Procedural Sound Wave

```c++
void UTic80AudioComponent::StartTic80Audio()
{

	if (ProceduralSoundWave && !bIsPlaying)
	{
		Play();
		bIsPlaying = true;
	}
}

void UTic80AudioComponent::StopTic80Audio()
{
	if (bIsPlaying)
	{
		Stop();
		bIsPlaying = false;
	}
}

void UTic80AudioComponent::Init() {
	ProceduralSoundWave = NewObject<USoundWaveProcedural>(this);
	if (ProceduralSoundWave)
	{
		ProceduralSoundWave->SetSampleRate(TIC80_SAMPLERATE);
		ProceduralSoundWave->NumChannels = TIC80_SAMPLE_CHANNELS;
		ProceduralSoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
		ProceduralSoundWave->SoundGroup = SOUNDGROUP_Default;
		ProceduralSoundWave->bLooping = true;

		// Set up the audio generation callback
		ProceduralSoundWave->OnSoundWaveProceduralUnderflow.BindUObject(
			this, &UTic80AudioComponent::OnAudioGenerate);

		// Set this as the sound to play
		SetSound(ProceduralSoundWave);
	}
}

void UTic80AudioComponent::OnAudioGenerate(USoundWaveProcedural* ProceduralWave, int32 SamplesRequired)
{
	if (!Tic80Instance || !ProceduralWave)
	{
		// Generate silence if no TIC-80 instance
		const int32 NumBytes = SamplesRequired * sizeof(int16);
		TArray<uint8> SilentBuffer;
		SilentBuffer.SetNumZeroed(NumBytes);
		ProceduralWave->QueueAudio(SilentBuffer.GetData(), NumBytes);
		return;
	}

	// Process TIC-80 audio
	const int32 NumBytes = SamplesRequired * sizeof(int16);
	ByteAudioBuffer.SetNumUninitialized(NumBytes);
	ProcessTic80Audio(ByteAudioBuffer, NumBytes);

	// Queue the processed audio
	ProceduralWave->QueueAudio(ByteAudioBuffer.GetData(), NumBytes);
}

void UTic80AudioComponent::ProcessTic80Audio(TArray<uint8>& OutAudioData, int32 NumBytes)
{
	// Get audio data from TIC-80
	Tic80Instance->GetAudioData(Tic80AudioBuffer);

	// Convert from int16 to uint8 bytes
	const int32 Tic80Samples = Tic80AudioBuffer.Num();
	const int32 BytesFromTic80 = Tic80Samples * sizeof(int16);
	const int32 BytesNeeded = NumBytes;

	OutAudioData.SetNumZeroed(BytesNeeded);
	
	if (Tic80Samples > 0 && BytesFromTic80 > 0)
	{
		// Copy int16 samples as raw bytes
		const int32 BytesToCopy = FMath::Min(BytesFromTic80, BytesNeeded);
		FMemory::Memcpy(OutAudioData.GetData(), Tic80AudioBuffer.GetData(), BytesToCopy);
	}
}
```