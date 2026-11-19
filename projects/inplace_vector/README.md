# inplace_vector

`Inplace::Vector<T, N>`: a list with room for up to `N` items inside it. Adding never allocates, and a copy is one block of memory: trivially copyable when `T` is. It's C++26's `std::inplace_vector` in miniature (the parts the games use, with the same names), to be swapped for the standard one when every compiler here has it. The Mac's doesn't yet.

Made for Cosmic Hex's game state. A bot copies the state at the start of every simulated game, and with `std::vector` each copy was a dozen or more allocations, over half a millisecond a copy on a DS (melonDS).

Pushing past the room is a bug in the caller: it stops the program with a message, never a silent drop. Choose `N` from the real limits (for the rules, the board's and the cards' counts).

```cpp
Inplace::Vector<int, 32> deck{0, 19, 23};
deck.push_back(4);
Inplace::erase(deck, 19);
```

Header-only, no dependencies. `bazel test //projects/inplace_vector:all`. For a console's Makefile build, stage `:sources` and add `inplace_vector/include` to `INCLUDES`.
