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
  Animation", SIGGRAPH 1987 (Computer Graphics 21:4, pp. 35–44)**, read whole
  (the book folder's copy):
  - *Timing* (p. 37): spacing actions defines weight and size; heavier things
    take longer to speed up and slow down.
  - *Anticipation* (p. 38): an action has a preparation, the action and its end;
    without the preparation actions look "abrupt, stiff and unnatural". Its
    size sets how fast the action can be: a well-anticipated action can be
    very quick (Wally B.'s zip-off is 3 or 4 frames, after a long wind-up); an
    unanticipated fast one is missed. A slow action needs little.
  - *Follow-through and overlapping action* (p. 39): actions are carried past
    their stop; loose parts drag behind and settle later, the farther and
    longer the heavier they are; "an action should never be brought to a
    complete stop before starting another", the next overlapping it.
  - *Slow in and out* (p. 40): spacing bunched at the extremes. Splines
    overshoot at extremes: Luxo Jr.'s base dipped through the floor before
    and after a hop until the spline was "locked" there. An overshoot can
    read as anticipation, but usually it's a fault.
  - *Arcs* (p. 41): the path between extremes is "always described by an arc",
    nature's most economical route; straight paths look stiff. The path's
    spline should be kept apart from the timing's, so a fast move keeps its
    arc and the timing can change without changing the path.
  - *Squash and stretch* (p. 36): rigid things (dishes, pans) stay rigid; living
    things change shape.
  - *Exaggeration* (p. 41): the essence of an idea accentuated, balanced against
    things kept natural so the whole still reads as real.
  - *Secondary action* (p. 42): results from the main action and stays
    subordinate to it.
- **Jonasson and Purho, "Juice it or lose it" (GDC 2012)**: a plain game made
  lively by easing every move (tweening position, rotation and scale) and by
  layering small effects (flashes, shakes, particles, sound) on each event.

## The rules

1. **Nothing moves in a straight line at an even speed.** Moves ease in and
   out (`Motion::Curve::Gentle`), and anything that travels follows an arc
   (`Motion::arc`): a tile lifts up and over, a card is thrown in a curve.
2. **A wind-up before a move away** (anticipation): a tile dips a little into
   the board before it lifts off (`Curve::Anticipate`, back about a tenth).
   Small, so it reads as weight, not as a stumble; the bigger the wind-up, the
   faster the move after it can be.
3. **A settle after a stop** (follow-through): pieces arriving overshoot a
   hair and come back (`Curve::Settle`): a token dropped on, a card landing in
   the hand, a die landing. Heavier things settle longer. Overshoot only where
   it's meant (Lasseter's Luxo base going through the floor is the warning).
4. **Overlap**: the next move starts before the last has quite finished,
   rather than everything stopping dead in between (cards thrown one a little
   after the other; a card leaving as the die fades).
5. **Rigid things stay rigid.** Tiles and cards don't squash; their liveliness
   comes from their path, turn and timing. Soft or small things (a number
   token, a die's pop) may scale a little.
6. **Timing says weight and importance.** A discovery, the game's big moment,
   takes its time (about 4 s in all); a button's press glow a quarter of a
   second; small things are quick.
7. **Reward in more than one way at once** (Schell): a discovery turns over,
   drops its token, names itself and says what it gives; the home's pay shows
   its die, its face, the card flying to your hand, the count going up.
8. **Path and timing apart** (Lasseter p. 41): a move's path through space
   is one curve, its speed along it another, so either can change alone. The
   baked curves of the animation lab follow this.
9. **Never at the cost of clarity** (Lasseter's staging): one thing moves at a
   time where it matters, the game holds while it does, and the words stay
   readable and still.
