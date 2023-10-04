add_executable(uart_test
        uart_test.cpp
)

target_link_libraries(uart_test pico_stdlib)
pico_add_extra_outputs(uart_test)
