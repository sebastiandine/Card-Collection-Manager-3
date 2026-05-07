#pragma once

// Small utility for converting embedded SVG icons into wxBitmap. Used by the
// side panel and the magic card list to render the foil / signed / altered
// flag icons (sourced from react-icons artwork). Also hosts the
// toolbar glyphs (vscode-codicons, matching react-icons/vsc-style buttons).

#include <wx/bitmap.h>

namespace ccm::ui {

// SVG templates for the per-game flag icons. Original sources:
//   - foil          -> IoSparklesSharp (Ionicons 5, MIT)        [Magic]
//   - signed        -> BsPencilFill    (Bootstrap Icons, MIT)
//   - altered       -> BsPaletteFill   (Bootstrap Icons, MIT)
//   - holo          -> IoSparklesSharp (Ionicons 5, MIT)        [Pokemon, mirrors original
//                                                                IconHolo from PokemonTable.tsx]
//   - firstEdition  -> rebuilt 1. Edition badge (CCM2 IconPokemonFirstEdition.tsx)
// The fill color is parameterized via a `@FILL@` placeholder so callers can
// choose the actual color at render time (e.g. system text vs. system
// highlight-text). NanoSVG cannot resolve CSS `currentColor`, so we have to
// bake the color into the SVG ourselves before parsing.
extern const char* const kSvgFoil;
extern const char* const kSvgSigned;
extern const char* const kSvgAltered;
extern const char* const kSvgHolo;
extern const char* const kSvgFirstEdition;

// Toolbar actions — glyphs match the original `src/pages/index.tsx` imports from
// `react-icons/vsc` (VscAdd / VscEdit / VscTrash). Embedded SVGs are sourced
// from Microsoft's vscode-codicons (MIT), same vector artwork as VS Code's
// codicon font used by react-icons.
extern const char* const kSvgToolbarAdd;
extern const char* const kSvgToolbarEdit;
extern const char* const kSvgToolbarDelete;

// Rasterize an SVG template into a wxBitmap of `size`x`size` pixels. The
// `@FILL@` placeholder in the template is replaced with `fillHex` (any CSS
// color string accepted by NanoSVG, e.g. "#000000" or "white").
// Backed by wxBitmapBundle::FromSVG, which uses NanoSVG (built in to our
// wxWidgets - configure log: `wxUSE_NANOSVG: builtin`).
wxBitmap svgIconBitmap(const char* svg, int size, const char* fillHex = "#000000");

// Same SVG, rasterized at `iconSize` and composited onto a transparent
// `container` canvas with the icon centered. Useful for wxListCtrl image
// lists where header bitmaps render left-anchored on MSW: padding the
// bitmap to the column width visually centers the icon under the header.
wxBitmap paddedSvgIcon(const char* svg, int iconSize, wxSize container,
                       const char* fillHex = "#000000");

}  // namespace ccm::ui
