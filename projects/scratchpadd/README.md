*ScratchPadd*

ScratchPadd is a simple tool for managing message passing between discrete subsystems of a single process. 

All padds must be implemented with a contructor that takes an instance that implements the `System`` interface

Use the following command to format the files using clang format
```
bazel run --jobs=1 --test_output=all //projects/scratchpadd:clang_format
```

TODO

repeater virtual method should return an optional. nullopt means no interval, a value >0 means add the repeat() methods work to the work queue at that time interval. A 0 just means continuously add to the queue

***Possible Later Optimizations***

The padd name is a method call everywhere that returns a string. This could maybe be set once in the future and accessed from the instance var. The challenge was making soemitngn taht can be setup semi automatically be be accessible in the base destructor.