#include "ccm/ui/MagicSelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
// Detail-row keys local to the Magic implementation.
enum MagicDetailKey : int {
    kName = 0,
    kSet,
    kLanguage,
    kCondition,
    kAmount,
    kFoil,
    kSigned,
    kAltered,
};
}  // namespace

MagicSelectedCardPanel::MagicSelectedCardPanel(wxWindow* parent,
                                               ImageService& imageService,
                                               CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<MagicCard>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<MagicSelectedCardPanel::DetailRowSpec>
MagicSelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"Language",  kLanguage,  ""},
        {"Condition", kCondition, ""},
        {"Amount",    kAmount,    ""},
    };
}

std::vector<MagicSelectedCardPanel::FlagIconSpec>
MagicSelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgFoil,    "Foil",    kFoil},
        {kSvgSigned,  "Signed",  kSigned},
        {kSvgAltered, "Altered", kAltered},
    };
}

std::string MagicSelectedCardPanel::detailValueFor(const MagicCard& card,
                                                   DetailKey key) const {
    switch (key) {
    case kName:      return card.name;
    case kSet:       return card.set.name;
    case kLanguage:  return std::string(to_string(card.language));
    case kCondition: return std::string(to_string(card.condition));
    case kAmount:    return std::to_string(card.amount);
    case kNoteKey:   return card.note;
    }
    return {};
}

bool MagicSelectedCardPanel::isFlagSet(const MagicCard& card, DetailKey key) const {
    switch (key) {
    case kFoil:    return card.foil;
    case kSigned:  return card.signed_;
    case kAltered: return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
MagicSelectedCardPanel::previewKey(const MagicCard& card) const {
    return {card.name, card.set.id, std::string{}};
}

}  // namespace ccm::ui
