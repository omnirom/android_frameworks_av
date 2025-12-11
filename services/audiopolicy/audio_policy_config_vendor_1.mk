$(call soong_config_set_bool,frameworks_av,use_aosp_audio_effects_config,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_audio_policy_volumes,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_default_volume_tables,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_r_submix_audio_policy_configuration,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_surround_sound_configuration_5_0,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_usb_audio_policy_configuration,true)

PRODUCT_PACKAGES += \
    aosp_audio_effects.xml \
    aosp_audio_policy_volumes.xml \
    aosp_default_volume_tables.xml \
    aosp_r_submix_audio_policy_configuration.xml \
    aosp_surround_sound_configuration_5_0.xml \
    aosp_usb_audio_policy_configuration.xml
