
#include "hello.h"

 auto makeGreeting() -> std::string {
    std::array greetingArray{"hello","hi","how are ya","greetings"};
    std::mt19937 rng(time(0)); // seed the random number generator with the current time
    std::uniform_int_distribution<int> dist(0, 3); // initialize a uniform distribution over [0, 3]

    // generate a random index using the uniform distribution and the random number generator
    int randomIndex = dist(rng);

    // select a random string from the list using the random index
    return greetingArray[randomIndex];
}