// Every card in the agreed deck has its drawing; resources theirs
#include <CosmicHex/Handheld/CardsHeld.hpp>

#include <gtest/gtest.h>

#include <set>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(CardsHeldTest, EveryCardInTheDeckHasItsDrawing) {
  const State state = newGame(4);
  std::set<int> drawn;
  for (int id : state.deck) {
    const auto card = cardOf(id);
    ASSERT_TRUE(card.has_value()) << "card " << id;
    if (card)
      drawn.insert(static_cast<int>(*card));
  }
  EXPECT_EQ(drawn.size(), 7u); // (four action cards and three Charters)
  EXPECT_EQ(cardOf(Resource::Water), Art::Card::Water);
  EXPECT_FALSE(cardOf(5).has_value());
}
