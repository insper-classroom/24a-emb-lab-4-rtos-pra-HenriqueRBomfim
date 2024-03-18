add_executable(main
        main.c
)

target_link_libraries(main pico_stdlib oled1_lib freertos hardware_rtc)
pico_add_extra_outputs(main)
