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

# Yu-Gi-Oh! preview fallback image (used when network card-back URLs fail).
add_custom_command(TARGET ccm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ccm>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ui_wx/assets/ygo_card_back.png"
            "$<TARGET_FILE_DIR:ccm>/assets/ygo_card_back.png")
