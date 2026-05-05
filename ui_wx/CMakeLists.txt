# ccm_ui_wx: wxWidgets adapter. The only target that depends on wx::wx.
# Replace this directory with a different toolkit (Qt, Dear ImGui, ...) without
# touching ccm_core.

add_library(ccm_ui_wx STATIC
    src/MainFrame.cpp
    src/MagicCardListPanel.cpp
    src/SelectedCardPanel.cpp
    src/CardEditDialog.cpp
    src/SettingsDialog.cpp
    src/ImageViewerDialog.cpp
    src/SvgIcons.cpp
    src/Theme.cpp
)

target_include_directories(ccm_ui_wx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(ccm_ui_wx
    PUBLIC
        ccm_core
        wx::wx
    # Intentionally NOT linking ccm_warnings here: wxWidgets headers raise
    # spurious diagnostics under -Wpedantic / -Wshadow that we'd have to
    # suppress per-call. The strict warning set is reserved for ccm_core.
)

target_compile_features(ccm_ui_wx PUBLIC cxx_std_20)
