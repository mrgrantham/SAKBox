#!/usr/bin/env bash
# Sets up an Ubuntu 24.04 ARM64 VM for check.py, as root (check.py ubuntu
# --setup runs it): the compilers and packages the Linux check's Docker
# images have, Bazelisk, and a desktop that logs in by itself on X11 and
# never blanks, for the tests that open a window. Running it again is
# harmless.
#
# Usage: ubuntu_setup.sh USER (the VM's desktop user)
set -euo pipefail
user="$1"

# GCC 14 and Clang 18 with GCC 14's standard library, as the Docker images
# have; the X11, Mesa, Vulkan and ALSA packages the top-level README's Linux
# notes list; Python for the repo's tools; rsync to copy the checkout
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq g++-14 clang-18 libstdc++-14-dev \
  libxinerama-dev libxrandr-dev libxcursor-dev libxi-dev \
  libglu1-mesa-dev libgl1-mesa-dev libegl1-mesa-dev libasound2-dev \
  libvulkan1 mesa-vulkan-drivers \
  python3 git curl ca-certificates rsync >/dev/null

# Bazelisk, which runs the Bazel version in .bazelversion. Downloaded beside
# it and moved into place, which works while it's running; prlctl exec's
# root has umask 077, so it's made runnable by everyone.
curl -fsSL -o /usr/local/bin/bazelisk.new \
  "https://github.com/bazelbuild/bazelisk/releases/download/v1.25.0/bazelisk-linux-$(dpkg --print-architecture)"
chmod 755 /usr/local/bin/bazelisk.new
mv /usr/local/bin/bazelisk.new /usr/local/bin/bazelisk

# Log the desktop user in at boot, on X11 (Ubuntu's default is Wayland), so
# windows can open with no one at the VM
conf=/etc/gdm3/custom.conf
if ! grep -q "^AutomaticLogin=$user" "$conf"; then
  sed -i "s/^\[daemon\]/[daemon]\nAutomaticLoginEnable=true\nAutomaticLogin=$user\nWaylandEnable=false/" "$conf"
  echo "Restart the VM once for the automatic login to take effect."
fi
# Never blank the screen (GNOME's default is after 15 idle minutes), so a
# window opened in a test or for a screenshot is seen, from its session
# (check.py runs this with the user logged in)
uid=$(id -u "$user")
sudo -u "$user" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$uid/bus" \
  gsettings set org.gnome.desktop.session idle-delay 0
sudo -u "$user" DISPLAY=:0 XAUTHORITY="/run/user/$uid/gdm/Xauthority" xset dpms force on
echo "Set up."
