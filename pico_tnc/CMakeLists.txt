add_library(pico_tnc
        tnc.c
        send.c
        # receive.c
        # decode.c
        filter.c
        bell202.c
        ax25.c
        unproto.c)

pico_generate_pio_header(pico_tnc ${CMAKE_CURRENT_LIST_DIR}/pio_dac.pio)

target_include_directories(pico_tnc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pico_tnc
		pico_stdlib
		hardware_adc
		hardware_dma
        hardware_pwm
        hardware_uart
        hardware_flash
		# For the dummy output:
		hardware_pio)