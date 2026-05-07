#include "ccm/ui/PokemonSelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
enum PokemonDetailKey : int {
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

PokemonSelectedCardPanel::PokemonSelectedCardPanel(wxWindow* parent,
                                                   ImageService& imageService,
                                                   CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<PokemonCard>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<PokemonSelectedCardPanel::DetailRowSpec>
PokemonSelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"Set #",     kSetNo,     ""},
        {"Language",  kLanguage,  ""},
        {"Condition", kCondition, ""},
        {"Amount",    kAmount,    ""},
    };
}

std::vector<PokemonSelectedCardPanel::FlagIconSpec>
PokemonSelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgHolo,         "Holo",        kHolo},
        {kSvgFirstEdition, "1. Edition",  kFirstEdition},
        {kSvgSigned,       "Signed",      kSigned},
        {kSvgAltered,      "Altered",     kAltered},
    };
}

std::string PokemonSelectedCardPanel::detailValueFor(const PokemonCard& card,
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

bool PokemonSelectedCardPanel::isFlagSet(const PokemonCard& card, DetailKey key) const {
    switch (key) {
    case kHolo:         return card.holo;
    case kFirstEdition: return card.firstEdition;
    case kSigned:       return card.signed_;
    case kAltered:      return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
PokemonSelectedCardPanel::previewKey(const PokemonCard& card) const {
    return {card.name, card.set.id, card.setNo};
}

}  // namespace ccm::ui
