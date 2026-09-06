#pragma once

// MagicDeckCheckPanel: paste a numbered deck list, analyze against the
// Magic collection by card name (all printings count), then show missing
// copies and owned copies (expandable when multiple variations exist).

#include "ccm/domain/MagicCard.hpp"
#include "ccm/services/MagicDeckCheck.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/panel.h>

#include <vector>

class wxBoxSizer;
class wxButton;
class wxScrolledWindow;
class wxSimplebook;
class wxStaticText;
class wxTextCtrl;

namespace ccm::ui {

class MagicDeckCheckPanel : public wxPanel {
public:
    explicit MagicDeckCheckPanel(wxWindow* parent);

    void setCollection(std::vector<MagicCard> cards);
    void applyTheme(const ThemePalette& palette);

private:
    void onAnalyze();
    void onClear();
    void showInputPage();
    void showResultPage();
    void rebuildResults();
    void refreshAnalysisIfActive();
    void addMissingRow(const MagicDeckMissingRow& row);
    void addOwnedRow(const MagicDeckOwnedRow& row);
    void addStatusIcon(wxWindow* parent, wxBoxSizer* sizer, const char* svg,
                       const char* fillHex);
    [[nodiscard]] wxString variationLabel(const MagicCard& card) const;

    std::vector<MagicCard> collection_;
    MagicDeckCheckResult   result_;
    bool                   showingResult_{false};
    ThemePalette           palette_{};

    wxSimplebook*     book_{nullptr};
    wxPanel*          inputPage_{nullptr};
    wxStaticText*     inputHint_{nullptr};
    wxTextCtrl*       pasteInput_{nullptr};
    wxButton*         analyzeBtn_{nullptr};

    wxPanel*          resultPage_{nullptr};
    wxButton*         clearBtn_{nullptr};
    wxScrolledWindow* scroll_{nullptr};
    wxBoxSizer*       resultSizer_{nullptr};
};

}  // namespace ccm::ui
