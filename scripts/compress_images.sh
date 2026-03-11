#!/bin/zsh

for file in pages/images/*/*.png; do
  sips -s format jpeg "$file" -Z 1024 --out "${file%.png}.jpg"
done
