# ccm: thin executable / composition root.
# Wires concrete adapters into the services and hands them to the UI.

add_executable(ccm
    main.cpp
)
set_target_properties(ccm PROPERTIES OUTPUT_NAME ccm3)

# Subsystem WINDOWS on Windows so we don't get a stray console.
if(WIN32)
    target_sources(ccm PRIVATE ccm.rc)
    set_target_properties(ccm PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(ccm
    PRIVATE
        ccm_core
        ccm_ui_wx
        ccm_warnings
)
