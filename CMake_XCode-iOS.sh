#!/usr/bin/env bash
SOURCE=$(cd ${0%/*}; pwd)
cmake -E make_directory ../AtomicGameEngine-iOS-XCode && cmake -E chdir ../AtomicGameEngine-iOS-XCode cmake "$SOURCE" -DIOS=1 -DATOMIC_DEV_BUILD=1 -G Xcode
echo "XCode project written to ../AtomicGameEngine-XCode"
