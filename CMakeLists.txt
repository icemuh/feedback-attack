cmake_minimum_required(VERSION 3.22)
project(HowlShield VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: สร้างไบนารี universal (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# AU ใช้ได้บน macOS เท่านั้น
if(APPLE)
    set(HOWL_FORMATS AU VST3 Standalone)
else()
    set(HOWL_FORMATS VST3 Standalone)
endif()

juce_add_plugin(HowlShield
    COMPANY_NAME                "icemuhhh"          # <- เปลี่ยนเป็นชื่อของคุณ
    BUNDLE_ID                   com.icemuhhh.howlshield
    PLUGIN_MANUFACTURER_CODE    Ycmp                   # 4 ตัว มีตัวพิมพ์ใหญ่อย่างน้อย 1
    PLUGIN_CODE                 Hwsh                   # 4 ตัว มีตัวพิมพ์ใหญ่ 1 ตัว และต้องไม่ซ้ำปลั๊กอินอื่น
    PRODUCT_NAME                "Howl Shielded"
    FORMATS                     ${HOWL_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx Filter
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "Howl Shield needs audio input to detect and suppress feedback.")

juce_generate_juce_header(HowlShield)

target_sources(HowlShield PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    FeedbackEngine.h)

target_compile_definitions(HowlShield PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HowlShield
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ปุ่ม SETTINGS ในหน้าต่าง Standalone (ทดลอง) เปิดด้วย:  cmake -B build -DHOWL_INAPP_SETTINGS=ON
# (ปิดไว้เป็นค่าเริ่มต้น เพื่อไม่ให้กระทบการบิลด์ VST3/AU)
option(HOWL_INAPP_SETTINGS "In-app audio settings button (Standalone only)" OFF)
if(HOWL_INAPP_SETTINGS)
    target_compile_definitions(HowlShield PUBLIC HOWL_INAPP_SETTINGS=1)
endif()
