cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(Software)

target_sources(app PRIVATE
  src/activity.c
  src/backlight.c
  src/ble_media.c
  src/buttons.c
  src/display_ui.c
  src/heart_rate.c
  src/haptics.c
  src/main.c
  src/power.c
)
