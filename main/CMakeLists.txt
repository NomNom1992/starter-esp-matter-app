idf_component_register(SRC_DIRS          "."
                       EMBED_FILES root.html images/logo.jpg
                       PRIV_INCLUDE_DIRS  ".")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
