#include "ccm/ui/DigiBattle99SelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
enum DigiBattle99DetailKey : int {
    kName = 0,
    kSet,
    kSetNo,
    kLanguage,
    kCondition,
    kAmount,
    kHolo,
    kFirstEdition,
    kSigned,
    kAltered,
};
}  // namespace

DigiBattle99SelectedCardPanel::DigiBattle99SelectedCardPanel(wxWindow* parent,
                                                             ImageService& imageService,
                                                             CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<DigiBattle99Card>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<DigiBattle99SelectedCardPanel::DetailRowSpec>
DigiBattle99SelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"Set #",     kSetNo,     ""},
        {"Language",  kLanguage,  ""},
        {"Condition", kCondition, ""},
        {"Amount",    kAmount,    ""},
    };
}

std::vector<DigiBattle99SelectedCardPanel::FlagIconSpec>
DigiBattle99SelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgHolo,         "Holo",        kHolo},
        {kSvgFirstEdition, "1. Edition",  kFirstEdition},
        {kSvgSigned,       "Signed",      kSigned},
        {kSvgAltered,      "Altered",     kAltered},
    };
}

std::string DigiBattle99SelectedCardPanel::detailValueFor(const DigiBattle99Card& card,
                                                          DetailKey key) const {
    switch (key) {
    case kName:         return card.name;
    case kSet:          return card.set.name;
    case kSetNo:        return card.setNo;
    case kLanguage:     return std::string(to_string(card.language));
    case kCondition:    return std::string(to_string(card.condition));
    case kAmount:       return std::to_string(card.amount);
    case kNoteKey:      return card.note;
    }
    return {};
}

bool DigiBattle99SelectedCardPanel::isFlagSet(const DigiBattle99Card& card,
                                              DetailKey key) const {
    switch (key) {
    case kHolo:         return card.holo;
    case kFirstEdition: return card.firstEdition;
    case kSigned:       return card.signed_;
    case kAltered:      return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
DigiBattle99SelectedCardPanel::previewKey(const DigiBattle99Card& card) const {
    // Middle slot is Set.name (pack display name) for digimoncard.io pack=.
    return {card.name, card.set.name, card.setNo};
}

}  // namespace ccm::ui
