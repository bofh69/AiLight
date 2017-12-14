#!/bin/sh

if [ $# -ne "1" ]; then
  echo "Usage: $0 <version>"
  exit 1
fi

echo Changing to version $1

sed -i 's/"version": "[^"]*",/"version": "'$1'",/' package.json ||
	echo "WARNING: Failed to change package.json"

sed -i 's/#define APP_VERSION "[^"]*"/#define APP_VERSION "'$1'"/' src/main.h ||
	echo "WARNING: Failed to change src/main.h"
