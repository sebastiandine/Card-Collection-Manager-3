#pragma once

// IconListCtrl
//
// `wxListCtrl` subclass that custom-draws icon sub-items so they are
// pixel-perfect centered within the cell, matching our themed-header icon
// centering exactly even after column resize.
//
// Why we need this:
//   - Native MSW `LVS_REPORT` sub-item image rendering anchors the image at
//     the cell's left edge with a small built-in inset. The header icons in
//     this app are centered via wx sizers, so the two never line up.
//   - We override `NM_CUSTOMDRAW` to paint icons ourselves at the exact
//     center of each icon sub-item rect.
//   - Default text cell rendering is left untouched.
//
// Rendering path (MSW):
//   We keep one premultiplied 32 bpp BGRA DIB section per (iconIdx, selected)
//   variant and composite it onto the listctrl's HDC with `AlphaBlend`
//   (`AC_SRC_OVER` + `AC_SRC_ALPHA`) from `NM_CUSTOMDRAW`. We deliberately
//   do **not** route through `ImageList_Draw` / `HIMAGELIST`: in our test
//   environment `ImageList_Draw` on an `ILC_COLOR32` list ignored the alpha
//   channel and the "transparent" canvas around each glyph painted as
//   opaque black behind the icon — see `ui_wx/AGENTS.md` convention 11.
//
// Usage:
//   1) Construct with the same wxListCtrl flags as before.
//   2) Call `setIconColumns(firstIconCol, count)` so the subclass knows which
//      sub-item indices it owns.
//   3) Call `setIconPredicate(...)` with a callback that decides if the icon
//      should be drawn for a given (row, iconIdx).
//   4) Call `setIconBitmaps(normal, selected)` with two equally-sized vectors
//      of pre-rendered icon bitmaps — one variant per selection state.
//
// This class is MSW-specific in behavior (NM_CUSTOMDRAW). On other platforms
// `MSWOnNotify` is a no-op override and the listctrl falls back to default
// rendering — which is fine because this app only ships on Windows.

#include <wx/bitmap.h>
#include <wx/listctrl.h>

#include <functional>
#include <utility>
#include <vector>

namespace ccm::ui {

class IconListCtrl : public wxListCtrl {
public:
    using wxListCtrl::wxListCtrl;

    using IconPredicate = std::function<bool(long row, int iconIdx)>;

    ~IconListCtrl() override;

    void setIconColumns(int firstIconCol, int iconCount) noexcept {
        firstIconCol_ = firstIconCol;
        iconColCount_ = iconCount;
    }
    void setIconPredicate(IconPredicate p) { predicate_ = std::move(p); }

    // Replace the cached icon bitmaps. Both vectors must have the same size
    // (one entry per icon column). Internal premultiplied DIB cache rebuilds.
    void setIconBitmaps(std::vector<wxBitmap> normal,
                        std::vector<wxBitmap> selected);

protected:
    bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result) override;

private:
    int                  firstIconCol_{-1};
    int                  iconColCount_{0};
    IconPredicate        predicate_;
    std::vector<wxBitmap> normalBmps_;
    std::vector<wxBitmap> selectedBmps_;

    // Premultiplied BGRA DIB sections used as the source for `AlphaBlend`.
    // Stored as `void*` (HBITMAP) so the header stays free of `<windows.h>`.
    // Index layout:
    //   [0 .. iconColCount_)                    -> normal variants
    //   [iconColCount_ .. 2 * iconColCount_)    -> selected variants
    std::vector<void*> dibBitmaps_;
    int                dibWidth_{0};
    int                dibHeight_{0};

    void rebuildDibCache();
    void destroyDibCache();
};

}  // namespace ccm::ui
