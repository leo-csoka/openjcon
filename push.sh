#!/usr/bin/env bash
set -e

rm -f ./*.uf2
git add .
git commit -m "$1"
git push
