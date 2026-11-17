#include <CosmicHex/Art.hpp>

#include <cstddef>

namespace CosmicHex::Art {

namespace Recipes {
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

} // namespace CosmicHex::Art
