#include "ccm/ui/YuGiOhBandaiSelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
enum YuGiOhBandaiDetailKey : int {
    kName = 0,
    kSet,
    kSetNo,
    kRarity,
    kAmount,
    kCondition,
    kLanguage,
    kHolo,
    kSigned,
    kAltered,
};
}  // namespace

YuGiOhBandaiSelectedCardPanel::YuGiOhBandaiSelectedCardPanel(wxWindow* parent,
                                                             ImageService& imageService,
                                                             CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<YuGiOhBandaiCard>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<YuGiOhBandaiSelectedCardPanel::DetailRowSpec>
YuGiOhBandaiSelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"No.",       kSetNo,     ""},
        {"Rarity",    kRarity,    ""},
        {"Amount",    kAmount,    ""},
        {"Condition", kCondition, ""},
        {"Language",  kLanguage,  ""},
    };
}

std::vector<YuGiOhBandaiSelectedCardPanel::FlagIconSpec>
YuGiOhBandaiSelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgHolo,    "Holo",    kHolo},
        {kSvgSigned,  "Signed",  kSigned},
        {kSvgAltered, "Altered", kAltered},
    };
}

std::string YuGiOhBandaiSelectedCardPanel::detailValueFor(const YuGiOhBandaiCard& card,
                                                          DetailKey key) const {
    switch (key) {
    case kName:      return card.name;
    case kSet:       return card.set.name;
    case kSetNo:     return card.setNo;
    case kRarity:    return card.rarity;
    case kAmount:    return std::to_string(card.amount);
    case kCondition: return std::string(to_string(card.condition));
    case kLanguage:  return std::string(to_string(card.language));
    case kNoteKey:   return card.note;
    }
    return {};
}

bool YuGiOhBandaiSelectedCardPanel::isFlagSet(const YuGiOhBandaiCard& card,
                                              DetailKey key) const {
    switch (key) {
    case kHolo:    return card.holo;
    case kSigned:  return card.signed_;
    case kAltered: return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
YuGiOhBandaiSelectedCardPanel::previewKey(const YuGiOhBandaiCard& card) const {
    return {card.name, card.set.id, card.setNo};
}

}  // namespace ccm::ui
