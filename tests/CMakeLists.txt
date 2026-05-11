# Unit tests for ccm_core. Wx and infra-bound tests are out of scope here -
# everything in this target operates against the ccm_core ports/services using
# in-memory fakes, so the suite is fast and hermetic.

add_executable(ccm_core_tests
    fakes/InMemoryFileSystem.cpp

    fs_names_tests.cpp
    domain_json_tests.cpp
    image_service_tests.cpp
    collection_service_tests.cpp
    config_service_tests.cpp
    json_collection_repository_tests.cpp
    json_set_repository_tests.cpp
    local_image_store_tests.cpp
    set_service_tests.cpp
    magic_set_source_tests.cpp
    magic_card_preview_source_tests.cpp
    card_preview_service_tests.cpp
    local_preview_byte_cache_tests.cpp
    std_file_system_tests.cpp
    pokemon_set_source_tests.cpp
    pokemon_card_preview_source_tests.cpp
    icard_preview_source_tests.cpp
    yugioh_set_source_tests.cpp
    yugioh_card_preview_source_tests.cpp
    game_module_tests.cpp
    card_sorter_tests.cpp
    card_filter_tests.cpp
    http_get_mapping_tests.cpp
    cpr_http_client_tests.cpp

    main.cpp
)

target_include_directories(ccm_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ccm_core_tests
    PRIVATE
        ccm_core
        doctest::doctest
        ccm_warnings
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(ccm_core_tests)
