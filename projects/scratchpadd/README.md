*ScratchPadd*

ScratchPadd is a simple tool for managing message passing between discrete subsystems of a single process. 

Use the following command to format the files using clang format
```
bazel run --jobs=1 --test_output=all //projects/scratchpadd:clang_format
```

***Possible Later Optimizations***

The padd name is a method call everywhere that returns a string. This could maybe be set once in the future and accessed from the instance var. The challenge was making soemitngn taht can be setup semi automatically be be accessible in the base destructor.