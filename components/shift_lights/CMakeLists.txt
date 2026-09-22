idf_component_register(
    SRCS "shift_lights.c"
    INCLUDE_DIRS "include"
    REQUIRES driver led_strip
)

# Uncomment the next line ONLY for bench testing - it drops the sweep to
# 3000-7000 RPM so you can trigger it by hand, and the unit tests in test/
# (which assert the 9000-13000 values) will fail while it is enabled.
#target_compile_definitions(${COMPONENT_LIB} PRIVATE SHIFT_LIGHT_LOW_RPM_TEST)
