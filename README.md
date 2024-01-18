# SAKBox #
A Swiss-Army Knife Toolbox. Monorepo of various projects and their dependencies

## Examples ##
Simple examples programs using some of the dependencies that are defined in the `WORKSPACE` file



## Projects ##

__ScratchPadd__ - Library for message passing and thread management

## Third Party

The third party folder contains custom build files for a number of libraries that I thought might be useful for various projects I had in mind.


## For Linux ##
For linux you will need to build bazel if on arm
https://github.com/koenvervloesem/bazel-on-arm

Might need to install default jdk on raspbian rather than `sudo make requirements`

Then install bazel 6.4.0 (7.0.0 has issues with symlinks on linux)
`./scripts/build_bazel.sh 6.4.0`

For OpenGL reqs in scratchpadd and other projects you might need to install the following:
``` bash
sudo apt install libxinerama-dev
sudo apt install libxrandr-dev
sudo apt install libxcursor-dev
sudo apt install libxi-dev
sudo apt install libglu1-mesa-dev
```

### The below will vary between Ubuntu and Raspbian ###
You can install mesa an implementation of OpenGL. See indications in the answer.

Therefore, if you have an error such that:

Could NOT find OpenGL
Then, you want to install libgl1-mesa-dev. With debian-based system:

``` bash
sudo apt install libgl1-mesa-dev
```

For an error, such that:

Could NOT find OpenGL (missing: EGL)
Then, you want to install libegl1-mesa-dev. With debian-based system:

``` bash
sudo apt install libegl1-mesa-dev
```

