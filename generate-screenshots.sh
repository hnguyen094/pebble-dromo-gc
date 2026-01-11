#!/usr/bin/env bash

pebble clean && pebble build

if [[ ! -d screenshots ]]; then
  mkdir screenshots
fi

function scr() {
  pebble install --emulator "$1"
  sleep 1
  pebble screenshot ./screenshots/"$1".png
  pebble emu-set-timeline-quick-view on
  pebble emu-battery --percent 20 --charging
  sleep 1
  pebble screenshot ./screenshots/"$1"-1.png
  pebble emu-set-timeline-quick-view off
  # pebble emu-app-config
  # sleep 1
  # pebble screenshot ./screenshots/"$1"-feat2.png
  pebble kill
}

pebble wipe
scr aplite
scr chalk
scr diorite
scr flint
scr emery
