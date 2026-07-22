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

# Yu-Gi-Oh! / Digi-Battle preview fallback images and the Japanese Pokémon
# EN name catalog (used when network card-back URLs fail or no public URL
# exists / for JP English display names).
add_custom_command(TARGET ccm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ccm>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ccm>/assets/pokemon_jp_classic"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ui_wx/assets/ygo_card_back.png"
            "$<TARGET_FILE_DIR:ccm>/assets/ygo_card_back.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ui_wx/assets/digibattle99_card_back.png"
            "$<TARGET_FILE_DIR:ccm>/assets/digibattle99_card_back.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ui_wx/assets/pokemon_jp_en_catalog.json"
            "$<TARGET_FILE_DIR:ccm>/assets/pokemon_jp_en_catalog.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/ui_wx/assets/pokemon_jp_classic"
            "$<TARGET_FILE_DIR:ccm>/assets/pokemon_jp_classic")

# MinGW-w64: ship the toolchain runtime next to ccm3.exe so Explorer / IDE
# launches do not pick a mismatched libstdc++ off PATH (symptoms: Entry Point
# Not Found for __emutls_v._ZSt11__once_call in libcpr.dll).
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    get_filename_component(_ccm_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_ccm_rt_dll IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${_ccm_mingw_bin}/${_ccm_rt_dll}")
            add_custom_command(TARGET ccm POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ccm_mingw_bin}/${_ccm_rt_dll}"
                        "$<TARGET_FILE_DIR:ccm>/${_ccm_rt_dll}"
                VERBATIM)
        endif()
    endforeach()
endif()
