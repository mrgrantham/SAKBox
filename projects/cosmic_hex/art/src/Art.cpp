#include <CosmicHex/Art.hpp>

#include <array>
#include <cstddef>

namespace CosmicHex::Art {

namespace Recipes {
extern const unsigned char kCardBackRings[];
extern const std::size_t kCardBackRingsSize;
extern const unsigned char kCardBackPlanets[];
extern const std::size_t kCardBackPlanetsSize;
extern const unsigned char kCardWordsText[];
extern const std::size_t kCardWordsTextSize;
extern const unsigned char kIconRoll[];
extern const std::size_t kIconRollSize;
extern const unsigned char kIconBuild[];
extern const std::size_t kIconBuildSize;
extern const unsigned char kIconTrade[];
extern const std::size_t kIconTradeSize;
extern const unsigned char kIconShowResources[];
extern const std::size_t kIconShowResourcesSize;
extern const unsigned char kIconCards[];
extern const std::size_t kIconCardsSize;
extern const unsigned char kIconEndTurn[];
extern const std::size_t kIconEndTurnSize;
extern const unsigned char kIconOtherPlayers[];
extern const std::size_t kIconOtherPlayersSize;
extern const unsigned char kIconMoveList[];
extern const std::size_t kIconMoveListSize;
extern const unsigned char kIconConfirm[];
extern const std::size_t kIconConfirmSize;
extern const unsigned char kCardResourceMinerals[];
extern const std::size_t kCardResourceMineralsSize;
extern const unsigned char kCardResourceFood[];
extern const std::size_t kCardResourceFoodSize;
extern const unsigned char kCardResourceFuel[];
extern const std::size_t kCardResourceFuelSize;
extern const unsigned char kCardResourceWater[];
extern const std::size_t kCardResourceWaterSize;
extern const unsigned char kCardResourceCrystals[];
extern const std::size_t kCardResourceCrystalsSize;
extern const unsigned char kCardEnforcer[];
extern const std::size_t kCardEnforcerSize;
extern const unsigned char kCardInterstellarCartel[];
extern const std::size_t kCardInterstellarCartelSize;
extern const unsigned char kCardShipyardSurplus[];
extern const std::size_t kCardShipyardSurplusSize;
extern const unsigned char kCardSupplyDrop[];
extern const std::size_t kCardSupplyDropSize;
extern const unsigned char kCardCharterThreeOfAKind[];
extern const std::size_t kCardCharterThreeOfAKindSize;
extern const unsigned char kCardCharterOccupier[];
extern const std::size_t kCardCharterOccupierSize;
extern const unsigned char kCardCharterDeepSpaceRelay[];
extern const std::size_t kCardCharterDeepSpaceRelaySize;
extern const unsigned char kPlanetFaceDown[];
extern const std::size_t kPlanetFaceDownSize;
extern const unsigned char kPlanetMinerals[];
extern const std::size_t kPlanetMineralsSize;
extern const unsigned char kPlanetFood[];
extern const std::size_t kPlanetFoodSize;
extern const unsigned char kPlanetFuel[];
extern const std::size_t kPlanetFuelSize;
extern const unsigned char kPlanetWater[];
extern const std::size_t kPlanetWaterSize;
extern const unsigned char kPlanetCrystals[];
extern const std::size_t kPlanetCrystalsSize;
extern const unsigned char kPlanetMoon[];
extern const std::size_t kPlanetMoonSize;
extern const unsigned char kHome[];
extern const std::size_t kHomeSize;
extern const unsigned char kHomeExhaustedMinerals[];
extern const std::size_t kHomeExhaustedMineralsSize;
extern const unsigned char kHomeExhaustedFood[];
extern const std::size_t kHomeExhaustedFoodSize;
extern const unsigned char kHomeExhaustedFuel[];
extern const std::size_t kHomeExhaustedFuelSize;
extern const unsigned char kHomeExhaustedWater[];
extern const std::size_t kHomeExhaustedWaterSize;
extern const unsigned char kHomeExhaustedCrystals[];
extern const std::size_t kHomeExhaustedCrystalsSize;
extern const unsigned char kFindFaceDown[];
extern const std::size_t kFindFaceDownSize;
extern const unsigned char kFindNothing[];
extern const std::size_t kFindNothingSize;
extern const unsigned char kFindSalvage[];
extern const std::size_t kFindSalvageSize;
extern const unsigned char kFindRuins[];
extern const std::size_t kFindRuinsSize;
extern const unsigned char kFindOutpost[];
extern const std::size_t kFindOutpostSize;
extern const unsigned char kFindPirates[];
extern const std::size_t kFindPiratesSize;
extern const unsigned char kNumber2[];
extern const std::size_t kNumber2Size;
extern const unsigned char kNumber3[];
extern const std::size_t kNumber3Size;
extern const unsigned char kNumber4[];
extern const std::size_t kNumber4Size;
extern const unsigned char kNumber5[];
extern const std::size_t kNumber5Size;
extern const unsigned char kNumber6[];
extern const std::size_t kNumber6Size;
extern const unsigned char kNumber8[];
extern const std::size_t kNumber8Size;
extern const unsigned char kNumber9[];
extern const std::size_t kNumber9Size;
extern const unsigned char kNumber10[];
extern const std::size_t kNumber10Size;
extern const unsigned char kNumber11[];
extern const std::size_t kNumber11Size;
extern const unsigned char kNumber12[];
extern const std::size_t kNumber12Size;
} // namespace Recipes

std::string_view recipeOf(Face face) {
  const auto text = [](const unsigned char *bytes, std::size_t size) {
    return std::string_view(reinterpret_cast<const char *>(bytes), size);
  };
  switch (face) {
  case Face::PlanetFaceDown:
    return text(Recipes::kPlanetFaceDown, Recipes::kPlanetFaceDownSize);
  case Face::PlanetMinerals:
    return text(Recipes::kPlanetMinerals, Recipes::kPlanetMineralsSize);
  case Face::PlanetFood:
    return text(Recipes::kPlanetFood, Recipes::kPlanetFoodSize);
  case Face::PlanetFuel:
    return text(Recipes::kPlanetFuel, Recipes::kPlanetFuelSize);
  case Face::PlanetWater:
    return text(Recipes::kPlanetWater, Recipes::kPlanetWaterSize);
  case Face::PlanetCrystals:
    return text(Recipes::kPlanetCrystals, Recipes::kPlanetCrystalsSize);
  case Face::PlanetMoon:
    return text(Recipes::kPlanetMoon, Recipes::kPlanetMoonSize);
  case Face::Home:
    return text(Recipes::kHome, Recipes::kHomeSize);
  case Face::HomeExhaustedMinerals:
    return text(Recipes::kHomeExhaustedMinerals, Recipes::kHomeExhaustedMineralsSize);
  case Face::HomeExhaustedFood:
    return text(Recipes::kHomeExhaustedFood, Recipes::kHomeExhaustedFoodSize);
  case Face::HomeExhaustedFuel:
    return text(Recipes::kHomeExhaustedFuel, Recipes::kHomeExhaustedFuelSize);
  case Face::HomeExhaustedWater:
    return text(Recipes::kHomeExhaustedWater, Recipes::kHomeExhaustedWaterSize);
  case Face::HomeExhaustedCrystals:
    return text(Recipes::kHomeExhaustedCrystals, Recipes::kHomeExhaustedCrystalsSize);
  case Face::FindFaceDown:
    return text(Recipes::kFindFaceDown, Recipes::kFindFaceDownSize);
  case Face::FindNothing:
    return text(Recipes::kFindNothing, Recipes::kFindNothingSize);
  case Face::FindSalvage:
    return text(Recipes::kFindSalvage, Recipes::kFindSalvageSize);
  case Face::FindRuins:
    return text(Recipes::kFindRuins, Recipes::kFindRuinsSize);
  case Face::FindOutpost:
    return text(Recipes::kFindOutpost, Recipes::kFindOutpostSize);
  case Face::FindPirates:
    return text(Recipes::kFindPirates, Recipes::kFindPiratesSize);
  }
  return {};
}

std::string_view chipRecipeOf(int number) {
  const auto text = [](const unsigned char *bytes, std::size_t size) {
    return std::string_view(reinterpret_cast<const char *>(bytes), size);
  };
  switch (number) {
  case 2:
    return text(Recipes::kNumber2, Recipes::kNumber2Size);
  case 3:
    return text(Recipes::kNumber3, Recipes::kNumber3Size);
  case 4:
    return text(Recipes::kNumber4, Recipes::kNumber4Size);
  case 5:
    return text(Recipes::kNumber5, Recipes::kNumber5Size);
  case 6:
    return text(Recipes::kNumber6, Recipes::kNumber6Size);
  case 8:
    return text(Recipes::kNumber8, Recipes::kNumber8Size);
  case 9:
    return text(Recipes::kNumber9, Recipes::kNumber9Size);
  case 10:
    return text(Recipes::kNumber10, Recipes::kNumber10Size);
  case 11:
    return text(Recipes::kNumber11, Recipes::kNumber11Size);
  case 12:
    return text(Recipes::kNumber12, Recipes::kNumber12Size);
  default:
    return {}; // (a 7, or not a dice number)
  }
}

std::string_view iconRecipeOf(Icon which) {
  const auto text = [](const unsigned char *bytes, std::size_t size) {
    return std::string_view(reinterpret_cast<const char *>(bytes), size);
  };
  switch (which) {
  case Icon::Roll:
    return text(Recipes::kIconRoll, Recipes::kIconRollSize);
  case Icon::Build:
    return text(Recipes::kIconBuild, Recipes::kIconBuildSize);
  case Icon::Trade:
    return text(Recipes::kIconTrade, Recipes::kIconTradeSize);
  case Icon::ShowResources:
    return text(Recipes::kIconShowResources, Recipes::kIconShowResourcesSize);
  case Icon::Cards:
    return text(Recipes::kIconCards, Recipes::kIconCardsSize);
  case Icon::EndTurn:
    return text(Recipes::kIconEndTurn, Recipes::kIconEndTurnSize);
  case Icon::OtherPlayers:
    return text(Recipes::kIconOtherPlayers, Recipes::kIconOtherPlayersSize);
  case Icon::MoveList:
    return text(Recipes::kIconMoveList, Recipes::kIconMoveListSize);
  case Icon::Confirm:
    return text(Recipes::kIconConfirm, Recipes::kIconConfirmSize);
  }
  return {};
}

std::string_view cardRecipeOf(Card which) {
  const auto text = [](const unsigned char *bytes, std::size_t size) {
    return std::string_view(reinterpret_cast<const char *>(bytes), size);
  };
  switch (which) {
  case Card::Minerals:
    return text(Recipes::kCardResourceMinerals, Recipes::kCardResourceMineralsSize);
  case Card::Food:
    return text(Recipes::kCardResourceFood, Recipes::kCardResourceFoodSize);
  case Card::Fuel:
    return text(Recipes::kCardResourceFuel, Recipes::kCardResourceFuelSize);
  case Card::Water:
    return text(Recipes::kCardResourceWater, Recipes::kCardResourceWaterSize);
  case Card::Crystals:
    return text(Recipes::kCardResourceCrystals, Recipes::kCardResourceCrystalsSize);
  case Card::Enforcer:
    return text(Recipes::kCardEnforcer, Recipes::kCardEnforcerSize);
  case Card::InterstellarCartel:
    return text(Recipes::kCardInterstellarCartel, Recipes::kCardInterstellarCartelSize);
  case Card::ShipyardSurplus:
    return text(Recipes::kCardShipyardSurplus, Recipes::kCardShipyardSurplusSize);
  case Card::SupplyDrop:
    return text(Recipes::kCardSupplyDrop, Recipes::kCardSupplyDropSize);
  case Card::CharterThreeOfAKind:
    return text(Recipes::kCardCharterThreeOfAKind, Recipes::kCardCharterThreeOfAKindSize);
  case Card::CharterOccupier:
    return text(Recipes::kCardCharterOccupier, Recipes::kCardCharterOccupierSize);
  case Card::CharterDeepSpaceRelay:
    return text(Recipes::kCardCharterDeepSpaceRelay, Recipes::kCardCharterDeepSpaceRelaySize);
  }
  return {};
}

std::string_view fileOf(Card card) {
  static constexpr std::array<std::string_view, kCards> kFiles{
      "resource_minerals",       "resource_food",     "resource_fuel",
      "resource_water",          "resource_crystals", "enforcer",
      "interstellar_cartel",     "shipyard_surplus",  "supply_drop",
      "charter_three_of_a_kind", "charter_occupier",  "charter_deep_space_relay"};
  return kFiles[static_cast<std::size_t>(card)];
}

namespace {

std::string_view trimmed(std::string_view text) {
  while (!text.empty() && text.front() == ' ')
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\r'))
    text.remove_suffix(1);
  return text;
}

// Every card's words, read from cards.txt once ("file | title | short |
// words" a line; '#' lines and blank ones skipped)
const std::array<CardWords, kCards> &allWords() {
  static const std::array<CardWords, kCards> kWords = [] {
    std::array<CardWords, kCards> out{};
    std::string_view text(reinterpret_cast<const char *>(Recipes::kCardWordsText),
                          Recipes::kCardWordsTextSize);
    while (!text.empty()) {
      const std::size_t end = std::min(text.find('\n'), text.size());
      std::string_view line = text.substr(0, end);
      text.remove_prefix(std::min(end + 1, text.size()));
      if (trimmed(line).empty() || trimmed(line).front() == '#')
        continue;
      std::array<std::string_view, 4> fields{};
      for (std::size_t f = 0; f < fields.size(); ++f) {
        const std::size_t bar = f + 1 < fields.size() ? line.find('|') : line.size();
        fields[f] = trimmed(line.substr(0, bar));
        line.remove_prefix(std::min(bar + 1, line.size()));
      }
      for (int c = 0; c < kCards; ++c)
        if (fileOf(static_cast<Card>(c)) == fields[0])
          out[static_cast<std::size_t>(c)] = {fields[1], fields[2], fields[3]};
    }
    return out;
  }();
  return kWords;
}

} // namespace

CardWords wordsOf(Card card) { return allWords()[static_cast<std::size_t>(card)]; }

std::string_view cardBackRecipe(CardBack design) {
  return design == CardBack::Planets
             ? std::string_view(reinterpret_cast<const char *>(Recipes::kCardBackPlanets),
                                Recipes::kCardBackPlanetsSize)
             : std::string_view(reinterpret_cast<const char *>(Recipes::kCardBackRings),
                                Recipes::kCardBackRingsSize);
}

} // namespace CosmicHex::Art
