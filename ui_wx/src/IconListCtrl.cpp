#include "ccm/ui/IconListCtrl.hpp"

#include <wx/image.h>

#ifdef __WXMSW__
#include <windows.h>
#include <commctrl.h>
#endif

namespace ccm::ui {

#ifdef __WXMSW__

namespace {

// Convert a `wxBitmap` to a fresh 32 bpp BGRA DIB section with PREMULTIPLIED
// alpha, suitable for use as the source bitmap in a `AlphaBlend` call with
// `AC_SRC_OVER | AC_SRC_ALPHA`.
//
// Going through `wxImage` gives us a known-good straight-RGBA payload
// regardless of how the source `wxBitmap` was originally constructed
// (notably bitmaps from `wxBitmapBundle::FromSVG`). We then premultiply
// once, rounded.
//
// Math notes:
//   - The rounded form `(c * a + 127) / 255` is required. Plain `c * a` (no
//     divide) overflows the byte and pushes every channel toward 0xFF — the
//     "white icons" regression an earlier dev ran into when they tried to
//     premultiply manually. `(c * a) / 255` is also wrong for `c=a=0xFF`
//     (rounds to 254 instead of 255 and creates 1-bit dimming on opaque
//     pixels). The +127 form is the standard premultiply rounding.
HBITMAP makePremultipliedDib(const wxBitmap& bmp) {
    if (!bmp.IsOk()) return NULL;

    wxImage img = bmp.ConvertToImage();
    if (!img.IsOk()) return NULL;
    if (!img.HasAlpha()) img.InitAlpha();

    const int w = img.GetWidth();
    const int h = img.GetHeight();
    if (w <= 0 || h <= 0) return NULL;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;  // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = ::GetDC(NULL);
    void* dibPixels = nullptr;
    HBITMAP dib = ::CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS,
                                     &dibPixels, NULL, 0);
    ::ReleaseDC(NULL, screenDc);
    if (dib == NULL || dibPixels == nullptr) {
        if (dib != NULL) ::DeleteObject(dib);
        return NULL;
    }

    const unsigned char* rgb   = img.GetData();
    const unsigned char* alpha = img.GetAlpha();
    auto* dest = static_cast<unsigned char*>(dibPixels);
    const int pixels = w * h;
    const auto premul = [](unsigned c, unsigned a) -> unsigned char {
        return static_cast<unsigned char>((c * a + 127) / 255);
    };
    for (int p = 0; p < pixels; ++p) {
        const unsigned char r = rgb[p * 3 + 0];
        const unsigned char g = rgb[p * 3 + 1];
        const unsigned char b = rgb[p * 3 + 2];
        const unsigned char a = (alpha != nullptr) ? alpha[p] : 255;
        dest[p * 4 + 0] = premul(b, a);
        dest[p * 4 + 1] = premul(g, a);
        dest[p * 4 + 2] = premul(r, a);
        dest[p * 4 + 3] = a;
    }
    return dib;
}

}  // namespace

IconListCtrl::~IconListCtrl() {
    destroyDibCache();
}

void IconListCtrl::setIconBitmaps(std::vector<wxBitmap> normal,
                                  std::vector<wxBitmap> selected) {
    normalBmps_   = std::move(normal);
    selectedBmps_ = std::move(selected);
    rebuildDibCache();
}

void IconListCtrl::destroyDibCache() {
    for (void* p : dibBitmaps_) {
        if (p != nullptr) ::DeleteObject(static_cast<HBITMAP>(p));
    }
    dibBitmaps_.clear();
    dibWidth_  = 0;
    dibHeight_ = 0;
}

void IconListCtrl::rebuildDibCache() {
    destroyDibCache();
    if (normalBmps_.empty() || normalBmps_.size() != selectedBmps_.size()) {
        return;
    }
    dibWidth_  = normalBmps_.front().IsOk() ? normalBmps_.front().GetWidth()  : 0;
    dibHeight_ = normalBmps_.front().IsOk() ? normalBmps_.front().GetHeight() : 0;
    if (dibWidth_ <= 0 || dibHeight_ <= 0) return;

    dibBitmaps_.reserve(normalBmps_.size() + selectedBmps_.size());
    for (const auto& b : normalBmps_) {
        dibBitmaps_.push_back(static_cast<void*>(makePremultipliedDib(b)));
    }
    for (const auto& b : selectedBmps_) {
        dibBitmaps_.push_back(static_cast<void*>(makePremultipliedDib(b)));
    }
}

bool IconListCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result) {
    auto* hdr = reinterpret_cast<NMHDR*>(lParam);
    if (hdr != nullptr && hdr->code == NM_CUSTOMDRAW) {
        auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            *result = CDRF_NOTIFYITEMDRAW;
            return true;

        case CDDS_ITEMPREPAINT:
            *result = CDRF_NOTIFYSUBITEMDRAW;
            return true;

        case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
            const int col = cd->iSubItem;
            if (col >= firstIconCol_ && col < firstIconCol_ + iconColCount_) {
                // Let the default first paint background/selection, then we
                // overlay the centered icon in POSTPAINT.
                *result = CDRF_NOTIFYPOSTPAINT;
                return true;
            }
            *result = CDRF_DODEFAULT;
            return true;
        }

        case CDDS_SUBITEM | CDDS_ITEMPOSTPAINT: {
            const int col = cd->iSubItem;
            if (col < firstIconCol_ || col >= firstIconCol_ + iconColCount_) {
                *result = CDRF_DODEFAULT;
                return true;
            }
            if (!predicate_ || dibBitmaps_.empty()) {
                *result = CDRF_DODEFAULT;
                return true;
            }
            const long row     = static_cast<long>(cd->nmcd.dwItemSpec);
            const int  iconIdx = col - firstIconCol_;
            if (iconIdx < 0 || iconIdx >= iconColCount_) {
                *result = CDRF_DODEFAULT;
                return true;
            }
            if (!predicate_(row, iconIdx)) {
                *result = CDRF_DODEFAULT;
                return true;
            }
            const HWND lcHwnd = reinterpret_cast<HWND>(GetHandle());
            // `cd->nmcd.uItemState & CDIS_SELECTED` is unreliable in the
            // CDDS_SUBITEM | CDDS_ITEMPOSTPAINT stage on Windows — comctl32
            // does not always propagate the item's CDIS_* flags down into
            // sub-item draw stages, so we'd silently fall back to the normal
            // (dark) variant on selected rows. Query LVIS_SELECTED directly
            // off the listview, which is always accurate.
            const UINT lvState = ListView_GetItemState(lcHwnd, row, LVIS_SELECTED);
            const bool selected = (lvState & LVIS_SELECTED) != 0;

            // Sub-item bounds in client coords. Initialize the request as
            // documented for LVM_GETSUBITEMRECT: rc.top = sub-item index,
            // rc.left = which rect (LVIR_BOUNDS).
            RECT rc;
            rc.top  = col;
            rc.left = LVIR_BOUNDS;
            ::SendMessageW(lcHwnd,
                           LVM_GETSUBITEMRECT,
                           static_cast<WPARAM>(row),
                           reinterpret_cast<LPARAM>(&rc));

            const int cellCx = (rc.left + rc.right) / 2;
            const int cellCy = (rc.top  + rc.bottom) / 2;
            const int x      = cellCx - dibWidth_  / 2;
            const int y      = cellCy - dibHeight_ / 2;

            const std::size_t imgIdx =
                static_cast<std::size_t>(iconIdx) +
                (selected ? static_cast<std::size_t>(iconColCount_) : 0);
            if (imgIdx >= dibBitmaps_.size() || dibBitmaps_[imgIdx] == nullptr) {
                *result = CDRF_DODEFAULT;
                return true;
            }
            HBITMAP src = static_cast<HBITMAP>(dibBitmaps_[imgIdx]);

            HDC dstDc = cd->nmcd.hdc;
            HDC memDc = ::CreateCompatibleDC(dstDc);
            HGDIOBJ oldBmp = ::SelectObject(memDc, src);

            BLENDFUNCTION bf{};
            bf.BlendOp             = AC_SRC_OVER;
            bf.BlendFlags          = 0;
            bf.SourceConstantAlpha = 0xFF;
            bf.AlphaFormat         = AC_SRC_ALPHA;

            ::AlphaBlend(dstDc, x, y, dibWidth_, dibHeight_,
                         memDc, 0, 0, dibWidth_, dibHeight_, bf);

            ::SelectObject(memDc, oldBmp);
            ::DeleteDC(memDc);

            *result = CDRF_DODEFAULT;
            return true;
        }

        default:
            break;
        }
    }
    return wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
}

#else  // !__WXMSW__

IconListCtrl::~IconListCtrl() = default;

void IconListCtrl::setIconBitmaps(std::vector<wxBitmap> normal,
                                  std::vector<wxBitmap> selected) {
    normalBmps_   = std::move(normal);
    selectedBmps_ = std::move(selected);
}

void IconListCtrl::destroyDibCache() {}
void IconListCtrl::rebuildDibCache() {}

bool IconListCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result) {
    return wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
}

#endif  // __WXMSW__

}  // namespace ccm::ui
