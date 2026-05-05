#include "ccm/ui/SvgIcons.hpp"

#include <wx/bmpbndl.h>
#include <wx/image.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

namespace ccm::ui {

const char* const kSvgFoil = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <path fill="@FILL@" d="M208 512l-29.86-80.13L98 401.86 178.14 372 208 292l29.86 80.13L318 401.86 237.86 432zM382 269l-22.4-60.11L299.47 186.4 359.6 164l22.4-60.11 22.4 60.11 60.13 22.4-60.13 22.4zM160 192l-26.06-69.94L64 96l69.94-26.06L160 0l26.06 69.94L256 96l-69.94 26.06z"/>
</svg>)SVG";

const char* const kSvgSigned = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="@FILL@" d="M12.854.146a.5.5 0 0 0-.707 0L10.5 1.793 14.207 5.5l1.647-1.646a.5.5 0 0 0 0-.708zm.646 6.061L9.793 2.5 3.293 9H3.5a.5.5 0 0 1 .5.5v.5h.5a.5.5 0 0 1 .5.5v.5h.5a.5.5 0 0 1 .5.5v.5h.5a.5.5 0 0 1 .5.5v.207zm-7.468 7.468A.5.5 0 0 1 6 13.5V13h-.5a.5.5 0 0 1-.5-.5V12h-.5a.5.5 0 0 1-.5-.5V11h-.5a.5.5 0 0 1-.5-.5V10h-.5a.5.5 0 0 1-.175-.032l-.179.178a.5.5 0 0 0-.11.168l-2 5a.5.5 0 0 0 .65.65l5-2a.5.5 0 0 0 .168-.11z"/>
</svg>)SVG";

const char* const kSvgAltered = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="@FILL@" d="M12.433 10.07C14.133 10.585 16 11.15 16 8a8 8 0 1 0-8 8c1.996 0 1.826-1.504 1.649-3.08-.124-1.101-.252-2.237.351-2.92.465-.527 1.42-.237 2.433.07ZM8 5.5a1.5 1.5 0 1 1-3 0 1.5 1.5 0 0 1 3 0m-3 4a1.5 1.5 0 1 1-3 0 1.5 1.5 0 0 1 3 0m6-2a1.5 1.5 0 1 1-3 0 1.5 1.5 0 0 1 3 0M11 12a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3"/>
</svg>)SVG";

// vscode-codicons — MIT License (Microsoft). Paths mirror VscAdd /
// VscEdit / VscTrash from react-icons/vsc.
const char* const kSvgToolbarAdd = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="@FILL@" d="M8 1.5C8 1.22386 7.77614 1 7.5 1C7.22386 1 7 1.22386 7 1.5V7H1.5C1.22386 7 1 7.22386 1 7.5C1 7.77614 1.22386 8 1.5 8H7V13.5C7 13.7761 7.22386 14 7.5 14C7.77614 14 8 13.7761 8 13.5V8H13.5C13.7761 8 14 7.77614 14 7.5C14 7.22386 13.7761 7 13.5 7H8V1.5Z"/>
</svg>)SVG";

const char* const kSvgToolbarEdit = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="@FILL@" d="M14.236 1.76386C13.2123 0.740172 11.5525 0.740171 10.5289 1.76386L2.65722 9.63549C2.28304 10.0097 2.01623 10.4775 1.88467 10.99L1.01571 14.3755C0.971767 14.5467 1.02148 14.7284 1.14646 14.8534C1.27144 14.9783 1.45312 15.028 1.62432 14.9841L5.00978 14.1151C5.52234 13.9836 5.99015 13.7168 6.36433 13.3426L14.236 5.47097C15.2596 4.44728 15.2596 2.78755 14.236 1.76386ZM11.236 2.47097C11.8691 1.8378 12.8957 1.8378 13.5288 2.47097C14.162 3.10413 14.162 4.1307 13.5288 4.76386L12.75 5.54269L10.4571 3.24979L11.236 2.47097ZM9.75002 3.9569L12.0429 6.24979L5.65722 12.6355C5.40969 12.883 5.10023 13.0595 4.76117 13.1465L2.19447 13.8053L2.85327 11.2386C2.9403 10.8996 3.1168 10.5901 3.36433 10.3426L9.75002 3.9569Z"/>
</svg>)SVG";

const char* const kSvgToolbarDelete = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="@FILL@" d="M14 2H10C10 0.897 9.103 0 8 0C6.897 0 6 0.897 6 2H2C1.724 2 1.5 2.224 1.5 2.5C1.5 2.776 1.724 3 2 3H2.54L3.349 12.708C3.456 13.994 4.55 15 5.84 15H10.159C11.449 15 12.543 13.993 12.65 12.708L13.459 3H13.999C14.275 3 14.499 2.776 14.499 2.5C14.499 2.224 14.275 2 13.999 2H14ZM8 1C8.551 1 9 1.449 9 2H7C7 1.449 7.449 1 8 1ZM11.655 12.625C11.591 13.396 10.934 14 10.16 14H5.841C5.067 14 4.41 13.396 4.346 12.625L3.544 3H12.458L11.656 12.625H11.655ZM7 5.5V11.5C7 11.776 6.776 12 6.5 12C6.224 12 6 11.776 6 11.5V5.5C6 5.224 6.224 5 6.5 5C6.776 5 7 5.224 7 5.5ZM10 5.5V11.5C10 11.776 9.776 12 9.5 12C9.224 12 9 11.776 9 11.5V5.5C9 5.224 9.224 5 9.5 5C9.776 5 10 5.224 10 5.5Z"/>
</svg>)SVG";

namespace {

// Substitute every "@FILL@" occurrence in `tmpl` with `fill`.
std::string applyFill(const char* tmpl, const char* fill) {
    std::string s(tmpl);
    constexpr std::string_view kPlaceholder = "@FILL@";
    for (std::string::size_type pos = s.find(kPlaceholder);
         pos != std::string::npos;
         pos = s.find(kPlaceholder, pos + std::strlen(fill))) {
        s.replace(pos, kPlaceholder.size(), fill);
    }
    return s;
}

}  // namespace

wxBitmap svgIconBitmap(const char* svg, int size, const char* fillHex) {
    const auto filled = applyFill(svg, fillHex);
    const auto bundle = wxBitmapBundle::FromSVG(
        reinterpret_cast<const wxByte*>(filled.data()),
        filled.size(),
        wxSize(size, size));
    if (!bundle.IsOk()) {
        wxImage img(size, size);
        img.SetAlpha();
        if (auto* a = img.GetAlpha()) std::fill(a, a + size * size, 0);
        return wxBitmap(img);
    }
    return bundle.GetBitmap(wxSize(size, size));
}

wxBitmap paddedSvgIcon(const char* svg, int iconSize, wxSize container,
                       const char* fillHex) {
    const int cw = container.GetWidth();
    const int ch = container.GetHeight();

    wxImage canvas(cw, ch);
    canvas.SetAlpha();
    if (auto* a = canvas.GetAlpha()) std::fill(a, a + cw * ch, 0);

    wxBitmap iconBmp = svgIconBitmap(svg, iconSize, fillHex);
    wxImage  iconImg = iconBmp.ConvertToImage();
    if (!iconImg.HasAlpha()) iconImg.InitAlpha();

    const int dx = (cw - iconSize) / 2;
    const int dy = (ch - iconSize) / 2;
    canvas.Paste(iconImg, dx, dy, wxIMAGE_ALPHA_BLEND_COMPOSE);
    return wxBitmap(canvas);
}

}  // namespace ccm::ui
