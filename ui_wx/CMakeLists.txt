# ccm_ui_wx: wxWidgets adapter. The only target that depends on wx::wx.
# Replace this directory with a different toolkit (Qt, Dear ImGui, ...) without
# touching ccm_core.

add_library(ccm_ui_wx STATIC
    src/MainFrame.cpp
    src/BaseEvents.cpp

    src/MagicCardListPanel.cpp
    src/MagicSelectedCardPanel.cpp
    src/MagicCardEditDialog.cpp
    src/MagicGameView.cpp

    src/PokemonCardListPanel.cpp
    src/PokemonSelectedCardPanel.cpp
    src/PokemonCardEditDialog.cpp
    src/PokemonGameView.cpp
    src/YuGiOhCardListPanel.cpp
    src/YuGiOhSelectedCardPanel.cpp
    src/YuGiOhCardEditDialog.cpp
    src/YuGiOhGameView.cpp

    src/SettingsDialog.cpp
    src/SwitchCtrl.cpp
    src/ImageViewerDialog.cpp
    src/IconListCtrl.cpp
    src/SvgIcons.cpp
    src/Theme.cpp
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ccm/ui/AppVersion.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ccm/ui/AppVersion.hpp
    @ONLY
)

target_include_directories(ccm_ui_wx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

target_link_libraries(ccm_ui_wx
    PUBLIC
        ccm_core
        wx::wx
    # Intentionally NOT linking ccm_warnings here: wxWidgets headers raise
    # spurious diagnostics under -Wpedantic / -Wshadow that we'd have to
    # suppress per-call. The strict warning set is reserved for ccm_core.
)

# IconListCtrl uses AlphaBlend (msimg32) directly in NM_CUSTOMDRAW to render
# the per-row flag glyphs with proper transparency. Without msimg32 linked
# explicitly the linker fails on `AlphaBlend@44` even though gdi32 is pulled
# in transitively by wxWidgets.
if (WIN32)
    target_link_libraries(ccm_ui_wx PRIVATE msimg32)
endif()

target_compile_features(ccm_ui_wx PUBLIC cxx_std_20)
