// ScreenPadd.hpp - the screens, on the main thread: the stage (the 3D
// scene: the title, the setup's board, the game's board with the shows
// over it) and the touch panel (the menus, the flat map and its buttons),
// the pad and the stylus, and the game's flow from the title to a win.
// TablePadd plays the game; each new state comes as a TableSnapshot.
//
// Its members are defined by role (ScreenLayout.hpp says where each role
// draws):
//
//   ScreenPadd.cpp      starting, the frame loop, holds, the animations' pace
//   GameFlow.cpp        a snapshot's arrival, starting and stopping games,
//                       who goes first, the dice, the win
//   BoardDrawing.cpp    the stage's scenes: the title, the setup's board,
//                       the game's board and its pieces
//   ShowsOverBoard.cpp  what shows over the board: the HUD, banners,
//                       close-ups, the shows' updates, the pointer
//   TouchPanel.cpp      the touch panel's picture, the buttons and touch,
//                       your moves
//
// and the menus are their own class (MenuScreens).
#pragma once

#if defined(__NDS__)
#include <ScratchPadd/Scene/DS/Display.hpp>
#else
#include <ScratchPadd/Scene/3DS/Display.hpp>
#endif

#include "AppBasics.hpp"
#include "MemoryReadout.hpp"
#include "MenuScreens.hpp"
#include "ScreenLayout.hpp"
#include "TableMessages.hpp"

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/GamePictures.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/RollDice.hpp>
#include <CosmicHex/Handheld/ScorePops.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>
#include <CosmicHex/Handheld/TopShows.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>
#include <CosmicHex/Handheld/WinCelebration.hpp>
#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/ConsoleScreens/ConsoleScreens.hpp>
#include <ScratchPadd/FrameProfile.hpp>
#include <ScratchPadd/GameClock.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/PictureSlot/PictureSlot.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/StillFrames/StillFrames.hpp>
#include <ScratchPadd/TitleScene/TitleScene.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CosmicHex::Handheld::App {

namespace Input = ScratchPadd::Input;
namespace ConsoleScreens = ScratchPadd::ConsoleScreens;

class ScreenPadd : public ScratchPadd::Base {
public:
  explicit ScreenPadd(ScratchPadd::System *system);
  std::string name() override { return "ScreenPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  void starting() override;

private:
  // Why the game is held, if it is: one bit each. The table is told to hold
  // while any is set or you've paused, and to go on once none is
  enum class Hold : std::uint8_t {
    Start = 1,     // (the board coming down, who goes first)
    Menu = 2,      // (the pause menu)
    Info = 4,      // (Show resources, another player's panel)
    Discovery = 8, // (a tile shown big)
    HomePay = 16,  // (a home's pay)
    RollPay = 32,  // (a roll's pay)
    CardPlay = 64  // (an action card played)
  };
  // In a game, watching or playing
  enum class Mode : std::uint8_t { Watching, Playing };
  // Where each frame's time goes (the DS's stutter), shown on
  // the -watch builds: the touch panel, the dice, the stage (its
  // wait for the refresh included), and that wait
  // (and, the board's pieces, the renderer's own CPU time and
  // its overlay's, from Scene::FrameTimes, and of the touch panel, the
  // map and the buttons over it)
  enum : std::size_t {
    kProfileBottom,
    kProfileOut,
    kProfileDice,
    kProfileTop,
    kProfileWait,
    kProfilePieces,
    kProfileRenderer,
    kProfileOverlay,
    kProfileMap,
    kProfileButtons
  };
  // The gaps' meshes for the last two boards drawn: while the setup's
  // boards slide, both show at once (the user's: the old one's
  // outlines went missing during the slide), each with the tiles it's for
  struct BoardGaps {
    std::vector<CosmicHex::Hex> tiles;
    Scene::Mesh mesh;
  };
  static constexpr Scene::Colour kFuelOrange{235, 150, 80}; // (fuel's, as its card's band)
  // How long the board, come down, is looked at before who goes first
  static constexpr double kSecondsLookingAtBoard = 2;

  // ScreenPadd.cpp: starting, the frame loop, holds, the animations' pace
  void loadPieces();
  void frame();
  void zoomMap(const Input::Edges &pressed, bool zooming);
  void followInfoView();
  void followTouchPanel();
  void updateStart();
  void updateShows();
  void prepareMap();
  void holdFor(Hold why, bool on);
  void sendHold();
  void save(const char *file, std::string_view bytes);
  void paceAnimations();
  bool skippable() const;
  void skipAnimations();
  // The app's real clock, in seconds (menus, buttons)
  double now() const { return secondsSince(startedAt_); }
  // The shows' clock (what shows over the board): real time at the
  // Animations setting's pace (twice as fast when Quick), jumping ahead
  // when an animation's skipped; it runs while the game's held
  double showsNow() const { return showClock_.seconds(); }

  // GameFlow.cpp: a snapshot's arrival, starting and stopping games, who
  // goes first, the dice, the win
  void onSnapshot(const TableSnapshot &snapshot);
  bool startMotion(const TableSnapshot &snapshot);
  void startBanners(const TableSnapshot &snapshot);
  void startShows(const TableSnapshot &snapshot);
  void showGains(const TableSnapshot &snapshot);
  void followOnTouchPanel(const TableSnapshot &snapshot);
  void apply(const ScreenFlow::Step &step);
  void startGame();
  void startPlacingBanner(const State &state);
  void startFirstRoll();
  void endFirstRoll();
  bool yourRollWaits(const TableSnapshot &snapshot) const;
  void promptForYourRoll();
  void throwDice();
  void celebrate(const State &state);
  void updateCelebration();

  // BoardDrawing.cpp: the stage's scenes
  void drawStage();
  void drawTitle();
  float titleFadedIn() const;
  static Scene::Environment boardEnvironment(float flat = 0);
  float descent() const;
  void drawBoard(const State *shown);
  void drawSetup();
  void drawGaps(const State &state, const Scene::Transform &placement);
  Scene::Mesh gapsFor(const State &state);
  void drawShip(const ShipPose &pose, const Scene::Transform &placement, Scene::Colour colour,
                std::uint8_t alpha = 31);
  void drawPieces(const State &state, const Scene::Transform &placement);
  void drawTile(CosmicHex::Hex hex, std::optional<float> turn, CosmicHex::Art::Face down,
                CosmicHex::Art::Face up, const Scene::Transform &placement);
  void drawTiles(const State &state, const Scene::Transform &placement, float t);
  void drawStations(const State &state, const Scene::Transform &placement, float t);
  void drawShips(const State &state, const Scene::Transform &placement, float t);
  void drawSelectedShip();

  // ShowsOverBoard.cpp: what shows over the board for a while
  void drawShowsOverBoard(const TopFrame &frame);
  void drawText(const TopFrame &frame);
  void drawWatchReadouts(Scene::Overlay &overlay);
  void drawHeldTag(Scene::Overlay &overlay);
  int infoPlayer(const State &state) const;
  void drawBanner(Scene::Overlay &overlay);
  void drawBand(Scene::Overlay &overlay, const ScratchPadd::Banner::Look &look);
  void drawMoves(Scene::Overlay &overlay);
  void showProfile();
  void closeUpTile(CosmicHex::Art::Face face);
  void closeUpTileInto(ScratchPadd::PictureSlot &slot, CosmicHex::Art::Face face, int size);
  void closeUpCards(const std::vector<CosmicHex::Art::Card> &cards);
  void updateRollPay();
  void updatePick();
  void updateHand();
  void updateCardPlay();
  void updateHomePay();
  void updateDiscovery();
  void updatePointer();
  std::optional<CosmicHex::Corner> pointerCorner() const;
  bool topMoving() const;
  bool topIsStill() const;

  // TouchPanel.cpp: the touch panel's picture, the buttons and touch, your
  // moves
  void handleButtons(const Input::Edges &pressed, bool zooming = false);
  bool skipOrHurry(const Input::Edges &pressed, Input::Button confirm, Input::Button back);
  void pressInGame(const Input::Edges &pressed, bool zooming, Input::Button confirm,
                   Input::Button back);
  void pressMoveList(const Input::Edges &pressed, Input::Button confirm, Input::Button back);
  bool choosingAHex() const;
  std::vector<std::size_t> movesShown() const;
  void handleTouch(const Input::Touch &touch);
  void tapBoard();
  Input::Touch scripted(const Input::Touch &touch);
  bool onTheBoard() const;
  bool onAMenuScreen() const;
  void drawTouchPanel();
  std::optional<std::span<const Scene::Colour>> drawTouchPicture(bool &showingMap);
  void drawMapWithButtons();
  void presentTouchPanel(std::span<const Scene::Colour> drawn, bool showingMap);

  // The stage: its display and renderer, and what it draws with
  std::optional<Scene::Display> display_;
  std::optional<Scene::Renderer> renderer_;
  Scene::Mesh station_, colony_, ship_, flame_, stars_, hexRing_, cornerRing_, chevron_;
  // The pictures the board and the views over it draw with (TopViews)
  TopPictures pictures_;
  // Close-ups: the slots pictures shown big are drawn into (GamePictures)
  CloseUpSlots closeUps_;
  // The title scene
  std::optional<ScratchPadd::TitleScene::TitleScene> title_;
  Clock::time_point titleStarted_;

  // Measured, for the -watch builds: memory, and each frame's time
  MemoryReadout memory_;
  ScratchPadd::FrameProfile profile_{"btm", "out", "dice", "top", "wait",
                                     "pcs", "3d",  "ovl",  "map", "btn"};
  std::string profileLine_;
  double startSeconds_ = 0; // (the start's work: starting())

  // The game as last sent, and how it's shown moving
  std::optional<TableSnapshot> latest_;
  BoardMotion motion_;
  // The game's own time (Gregory 8.4): moves animate and dice roll on it,
  // and it stops while a watched game is paused
  ScratchPadd::GameClock gameClock_;
  double motionStarted_ = 0;
  float turn_ = 0;
  // The roll's physics dice (RollDice), and the table's seed last thrown
  RollDice roll_{kThisPlatform.diceStep, kThisPlatform.mostDiceSteps};
  Scene::Mesh die_;
  std::uint64_t thrownSeed_ = 0;
  bool rollAsked_ = false;    // (you've said to throw the dice: Roll or A)
  bool rollPrompted_ = false; // (the banner says ROLL THE DICE, until you do)

  // The pad and the clocks
  Input::Gamepad lastPad_;
  Clock::time_point startedAt_ = Clock::now(); // (the scripted taps' times)
  ScratchPadd::GameClock showClock_;

  // The touch panel: the flat board (BoardPicture), its buttons and
  // choosing screens (BottomScreen), written straight into the bottom
  // screen's one framebuffer when it changes
  std::optional<ConsoleScreens::BottomScreen> touchScreen_;
  std::optional<BoardPicture> picture_;
  std::optional<BottomScreen> bottomScreen_;
  std::optional<Canvas> touchCanvas_;
  std::optional<Clock::time_point> feedbackSince_;
  bool pressDrawn_ = false; // (a button's press glow drawn last frame)
  bool touchDirty_ = true, touchWasDown_ = false, dragged_ = false;
  std::array<int, 2> touchStart_{}, touchLast_{};
  // What's selected on the board (touched), and the list of moves' cursor
  std::optional<BoardPicture::Spot> selected_;
  bool tapPointer_ = false;                      // (a touch chose a corner: the pointer taps it)
  std::optional<CosmicHex::Corner> flatPointer_; // (where the map last drew the pointer)
  CameraFollow follow_;                          // (the game's camera, following the corner chosen)
  std::optional<CosmicHex::Corner> followedCorner() const;
  // The PAUSED banner's coming and going (an info view holding the game):
  // since when it's been held, or let go
  double heldChangedAt_ = -1e9;
  bool heldShown_ = false;
  int othersShown_ = -1; // (the other player last shown: which way the carousel turns)
  std::size_t cursor_ = 0;
  // What the buttons cover on the map, as last drawn; and whether the
  // touch panel shows the map as last drawn (so only what changes needs
  // copying to it)
  std::vector<BoardPicture::Area> overBoardAreas_;
  bool screenShowsMap_ = false;
  bool fading_ = false;       // (mid-fade: the map already drawn)
  bool fadeFromMenu_ = false; // (the touch panel fading from the setup's menu)

  // The turn banner (ScratchPadd::Banner)
  ScratchPadd::Banner banner_{static_cast<float>(kStageWidth)};
  std::optional<Pickers::ResourceChoice> bannerFor_; // (the choice its banner was for)
  // The info views (Show resources): which is shown, since when, and
  // whether it holds the game; the cards' pictures for them
  BottomScreen::Info shownInfo_ = BottomScreen::Info::None;
  CardsShow cards_;                      // (Show resources' cards in and off)
  std::optional<double> outlineMovedAt_; // (the turn's outline moving: since, on the shows' clock)
  int outlineFrom_ = 0;                  // (from this player's row)
  // What shows over the board for a while (TopShows; drawn by TopViews),
  // each on the shows' clock (showsNow())
  DiscoveryShow discovery_;         // (tiles turned over)
  DealtShow dealt_;                 // (the two cards dealt)
  PanelShow others_{kSecondsPanel}; // (another player's panel)
  FirstRollShow firstRoll_;         // (who goes first)
  HomePayShow homePay_;             // (a home's pay on its die)
  RollPayShow rollPay_;             // (a roll's pay, card by card)
  ScorePops scorePops_;             // (points and badges won, popping in)
  CardPlayShow cardPlay_;           // (an action card played, a Charter revealed)
  PickShow pick_;                   // (choosing a resource: the card in focus, big)
  HandShow hand_;                   // (your cards gained, dropping into your hand)
  CornerPointer pointer_;           // (the board's pointer)
  // Someone's won: their ships fly round above the board (WinCelebration)
  std::optional<WinCelebration> celebration_;
  ScratchPadd::StillFrames still_; // (when the stage can hold its last frame)
  // What the stage last showed (topIsStill): the table's snapshots
  // drawn, the camera's turn and the selected spot, and when
  std::uint64_t snapshots_ = 0, drawnSnapshots_ = 0;
  float drawnTurn_ = 0;
  std::optional<BoardPicture::Spot> drawnSelection_;
  Clock::time_point drawnAt_{};
  // The gaps between the tiles, for the last two boards drawn
  std::array<BoardGaps, 2> gaps_;
  std::size_t olderGaps_ = 0; // (which of gaps_ goes next)

  // Holding the game: why, and whether you've paused it (watching, with
  // A, so the PAUSED band shows)
  std::uint8_t holds_ = 0;
  bool youPaused_ = false;

  // Which screen (ScreenFlow), and the menus it shows
  ScreenFlow flow_;
  MenuScreens menus_{
      flow_,
      {.step = [this](ScreenFlow::Event event) { apply(flow_.on(event)); },
       .animationsChanged = [this] { paceAnimations(); },
       .save = [this](const char *file, std::string_view bytes) { save(file, bytes); }}};
  Mode mode_ = Mode::Watching;
  bool quitting_ = false;
  // Who's who: your name, the bots' (PlayerNames), made as each game starts
  PlayerNames names_;
  // The start of a game: the board coming down, the touch panel fading
  std::optional<Clock::time_point> descentStarted_;
  // The board come down, looked at a moment before who goes first
  std::optional<Clock::time_point> lookingSince_;
};

} // namespace CosmicHex::Handheld::App
