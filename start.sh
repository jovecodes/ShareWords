#!/bin/bash

echo "Quick starting project..."

read -p "Execuable name: " exe_name

if [ -z "$exe_name" ]
then
      echo "Execuable name defaulted to 'jovial_test'"
else
    sed -i "s/jovial_test/${exe_name}/g" CMakeLists.txt
    sed -i "s/jovial_test/${exe_name}/g" build.jov.sh
fi

echo
read -p "Delete .git folder? (y/n) " -r answer

if [ "$answer" == "y" ] || [ "$answer" == "Y" ]; then
    rm -rf .git
fi

mkdir build
cd build
cmake ..
cd ..

echo "Quick started!"
