#include "ccm/ui/YuGiOhSelectedCardPanel.hpp"

#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

namespace {
enum YuGiOhDetailKey : int {
    kName = 0,
    kSet,
    kSetNo,
    kRarity,
    kLanguage,
    kCondition,
    kAmount,
    kFirstEdition,
    kSigned,
    kAltered,
};
}  // namespace

YuGiOhSelectedCardPanel::YuGiOhSelectedCardPanel(wxWindow* parent,
                                                 ImageService& imageService,
                                                 CardPreviewService& cardPreview)
    : BaseSelectedCardPanel<YuGiOhCard>(parent, imageService, cardPreview) {
    buildLayout();
}

std::vector<YuGiOhSelectedCardPanel::DetailRowSpec>
YuGiOhSelectedCardPanel::declareDetailRows() const {
    return {
        {"Name",      kName,      "(no card selected)"},
        {"Set",       kSet,       ""},
        {"Set #",     kSetNo,     ""},
        {"Rarity",    kRarity,    ""},
        {"Language",  kLanguage,  ""},
        {"Condition", kCondition, ""},
        {"Amount",    kAmount,    ""},
    };
}

std::vector<YuGiOhSelectedCardPanel::FlagIconSpec>
YuGiOhSelectedCardPanel::declareFlagIcons() const {
    return {
        {kSvgFirstEdition, "1. Edition",  kFirstEdition},
        {kSvgSigned,       "Signed",      kSigned},
        {kSvgAltered,      "Altered",     kAltered},
    };
}

std::string YuGiOhSelectedCardPanel::detailValueFor(const YuGiOhCard& card,
                                                    DetailKey key) const {
    switch (key) {
    case kName:         return card.name;
    case kSet:          return card.set.name;
    case kSetNo:        return card.setNo;
    case kRarity:       return card.rarity;
    case kLanguage:     return std::string(to_string(card.language));
    case kCondition:    return std::string(to_string(card.condition));
    case kAmount:       return std::to_string(card.amount);
    case kNoteKey:      return card.note;
    }
    return {};
}

bool YuGiOhSelectedCardPanel::isFlagSet(const YuGiOhCard& card, DetailKey key) const {
    switch (key) {
    case kFirstEdition: return card.firstEdition;
    case kSigned:       return card.signed_;
    case kAltered:      return card.altered;
    }
    return false;
}

std::tuple<std::string, std::string, std::string>
YuGiOhSelectedCardPanel::previewKey(const YuGiOhCard& card) const {
    // Pack rarity and edition into the third tuple slot so the YGO preview
    // source can build Yugipedia file names without changing the generic
    // ICardPreviewSource interface. Format: "<setNo>||<rarity>||<1E|UE>".
    // Yugipedia per-printing scans need the edition stamp to disambiguate
    // 1st-Edition vs Unlimited reprints.
    const char* const editionTag = card.firstEdition ? "1E" : "UE";
    return {card.name, card.set.name,
            card.setNo + "||" + card.rarity + "||" + editionTag};
}

}  // namespace ccm::ui
