# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/usr/share/pico-sdk/tools/pioasm"
  "/home/mitch/code/typewriter-hacking/pi-pico/pioasm"
  "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm"
  "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/tmp"
  "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/src/PioasmBuild-stamp"
  "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/src"
  "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/src/PioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/src/PioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mitch/code/typewriter-hacking/pi-pico/pico-sdk/src/rp2_common/cyw43_driver/pioasm/src/PioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
