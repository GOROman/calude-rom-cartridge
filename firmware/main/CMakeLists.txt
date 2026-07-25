idf_component_register(
    SRCS "main.c" "cart_loader.c" "web_server.c" "joypad_out.c" "audio.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver
)
