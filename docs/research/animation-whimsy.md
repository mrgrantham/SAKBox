# Animation with some whimsy: what the sources say, and how the game uses it

Cosmic Hex's moving pieces (a discovered tile, the cards dealt, the dice, the
home's pay) should feel alive and a little playful, not mechanical. This note
gathers what the sources say and turns it into rules the code follows. The
code that applies a rule names it here.

## Sources

- **Schell, *The Art of Game Design*, 2nd ed., ch. 15, "Juiciness" and Lens #64,
  The Lens of Juiciness (pp. 264–265).** A juicy interface gives continuous
  feedback, shows "second-order motion" that follows from what the player does,
  and rewards the player in several ways at once ("how many ways am I
  simultaneously rewarding them?").
- **Lasseter, "Principles of Traditional Animation Applied to 3D Computer
  Animation", SIGGRAPH 1987 (Computer Graphics 21:4, pp. 35–44), as summarised
  by SIGGRAPH's HyperGraph pages** (the paper itself couldn't be fetched):
  - *Timing*: "the speed of an action ... gives meaning to movement"; heavier
    things take longer to speed up and slow down. Their head-turn example: 0
    in-betweens reads as a strong hit, 3 as dodging, 6 as seeing something
    inviting, 9 as thinking.
  - *Anticipation*: every action has a preparation, the action and its end;
    the preparation (a foot drawn back before a kick) helps the viewer follow a
    fast move and shows weight.
  - *Follow-through*: the end of an action carries on past its stop and
    settles; heavier parts "lag farther and stop slower"; a second action can
    start before the first ends (overlap).
  - *Slow in and out*: speed varies at the ends of a move rather than staying
    even.
  - *Arcs*: "the visual path of action for natural movement".
  - *Squash and stretch*: for soft things; rigid objects "remain rigid in
    motion", their volume kept.
  - *Secondary action*: "the action of an object resulting from another
    action"; *exaggeration*: "accentuating the essence of an idea".
- **Jonasson and Purho, "Juice it or lose it" (GDC 2012)**: a plain game made
  lively by easing every move (tweening position, rotation and scale) and by
  layering small effects (flashes, shakes, particles, sound) on each event.

## The rules

1. **Nothing moves in a straight line at an even speed.** Moves ease in and
   out (`Motion::Curve::Gentle`), and anything that travels follows an arc
   (`Motion::arc`): a tile lifts up and over, a card is thrown in a curve.
2. **A wind-up before a move away** (anticipation): a tile dips a little into
   the board before it lifts off (`Curve::Anticipate`, back about a tenth).
   Small, so it reads as weight, not as a stumble.
3. **A settle after a stop** (follow-through): pieces arriving overshoot a
   hair and come back (`Curve::Settle`): a token dropped on, a card landing in
   the hand, a die landing.
4. **Rigid things stay rigid.** Tiles and cards don't squash; their liveliness
   comes from their path, turn and timing. Soft or small things (a number
   token, a die's pop) may scale a little.
5. **Timing says weight and importance.** A discovery, the game's big moment,
   takes its time (about 4 s in all); a button's press glow a quarter of a
   second; small things are quick.
6. **Reward in more than one way at once** (Schell): a discovery turns over,
   drops its token, names itself and says what it gives; the home's pay shows
   its die, its face, the card flying to your hand, the count going up.
7. **Never at the cost of clarity** (Lasseter's staging): one thing moves at a
   time where it matters, the game holds while it does, and the words stay
   readable and still.
