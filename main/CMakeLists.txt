idf_component_register(
 SRCS "app_main.c"
      "app/app_controller.c"
      "app/dice_state.c"
      "dice/dice_model.c"
      "dice/dice_custom_runtime.c"
      "dice/dice_set_catalog.c"
      "dice/dice_rules.c"
      "dice/dice_set_archive.c"
      "hardware/dice_audio.c"
      "hardware/dice_battery.c"
      "hardware/dice_display_reset.c"
      "hardware/dice_imu.c"
      "hardware/dice_power.c"
      "hardware/dice_storage.c"
      "hardware/dice_transfer.c"
      "ui/dice_ui.c"
      "ui/dice_face_assets.c"
 INCLUDE_DIRS "." "app" "dice" "hardware" "ui"
 REQUIRES driver esp_driver_i2s esp_codec_dev esp_timer
          fatfs wear_levelling nvs_flash esp_tinyusb esp_pm cjson lua libpng zlib
 PRIV_REQUIRES esp_hw_support heap
)
