set(libs
  drv_stereo_camera_static
  ${adisp_static_libs}
  ${alog_static_libs}
  ${engine_static_libs}
  ${util_static_libs}
  opencv_core
  opencv_imgproc
  pthread)

if("${arch}" STREQUAL "arm"
  OR "${arch}" STREQUAL "arm-oemllib32-linux-gnueabi")
  set(libs ${libs} opencv_videoio opencv_imgcodecs)
elseif ("${arch}" STREQUAL "host")
  set(libs ${libs} opencv_highgui)
endif()

set(libs
  ${libs}
  pthread
  rt
  udev)

set(libs
  ${libs}
  usb-1.0)

add_executable(firmware_updater
  download_fx3.cpp
  libcyusb.cpp
  Sample_update.cpp)
target_link_libraries(firmware_updater ${libs})

set(install-dir "/opt/drivers/")
add_custom_target(load-firmware-updater
  DEPENDS firmware_updater
  COMMAND adb wait-for-device
  COMMAND adb shell mkdir -p ${install-dir}
  COMMAND adb shell chmod -R 755 /opt
  COMMAND adb push ${CMAKE_CURRENT_BINARY_DIR}/firmware_updater ${install-dir}
  COMMAND adb shell chmod +x ${install-dir}/firmware_updater
  COMMAND adb shell sync
  )

print_compile_flags()
