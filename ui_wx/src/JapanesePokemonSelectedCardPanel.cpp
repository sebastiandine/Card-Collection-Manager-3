#include "ccm/ui/JapanesePokemonSelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
enum JapanesePokemonDetailKey : int {
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

JapanesePokemonSelectedCardPanel::JapanesePokemonSelectedCardPanel(wxWindow* parent,
                                                   ImageService& imageService,
                                                   CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<JapanesePokemonCard>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<JapanesePokemonSelectedCardPanel::DetailRowSpec>
JapanesePokemonSelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"Set #",     kSetNo,     ""},
        {"Language",  kLanguage,  ""},
        {"Condition", kCondition, ""},
        {"Amount",    kAmount,    ""},
    };
}

std::vector<JapanesePokemonSelectedCardPanel::FlagIconSpec>
JapanesePokemonSelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgHolo,         "Holo",        kHolo},
        {kSvgFirstEdition, "1. Edition",  kFirstEdition},
        {kSvgSigned,       "Signed",      kSigned},
        {kSvgAltered,      "Altered",     kAltered},
    };
}

std::string JapanesePokemonSelectedCardPanel::detailValueFor(const JapanesePokemonCard& card,
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

bool JapanesePokemonSelectedCardPanel::isFlagSet(const JapanesePokemonCard& card, DetailKey key) const {
    switch (key) {
    case kHolo:         return card.holo;
    case kFirstEdition: return card.firstEdition;
    case kSigned:       return card.signed_;
    case kAltered:      return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
JapanesePokemonSelectedCardPanel::previewKey(const JapanesePokemonCard& card) const {
    return {card.name, card.set.id, card.setNo};
}

}  // namespace ccm::ui
