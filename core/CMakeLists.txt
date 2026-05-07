# ccm_core: UI-agnostic domain, services, ports, and infra adapters.
# This target MUST NOT depend on wxWidgets or any UI toolkit.

add_library(ccm_core STATIC
    src/domain/Enums.cpp
    src/domain/Set.cpp
    src/domain/MagicCard.cpp
    src/domain/PokemonCard.cpp
    src/domain/Configuration.cpp

    src/services/ConfigService.cpp
    src/services/ImageService.cpp
    src/services/SetService.cpp
    src/services/CardPreviewService.cpp
    src/services/CardSorter.cpp
    src/services/CardFilter.cpp

    src/infra/CprHttpClient.cpp
    src/infra/StdFileSystem.cpp
    src/infra/JsonSetRepository.cpp
    src/infra/LocalImageStore.cpp

    src/games/magic/MagicSetSource.cpp
    src/games/magic/MagicCardPreviewSource.cpp
    src/games/magic/MagicGameModule.cpp
    src/games/pokemon/PokemonSetSource.cpp
    src/games/pokemon/PokemonCardPreviewSource.cpp
    src/games/pokemon/PokemonGameModule.cpp

    src/util/FsNames.cpp
)

target_include_directories(ccm_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(ccm_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        cpr::cpr
        ccm_warnings
)

target_compile_features(ccm_core PUBLIC cxx_std_20)

# JsonCollectionRepository<T> and CollectionService<T> are header-only templates
# and live entirely under include/ccm/ - nothing to compile here for them.
