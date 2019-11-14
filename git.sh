#!/bin/bash


if [ "$1" == "pull" ]; then
    git pull origin master
    exit
fi

git add .
git commit -m "$1"
git push origin master
