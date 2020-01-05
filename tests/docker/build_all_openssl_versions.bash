#!/bin/bash

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")

TEMPLATE_FILENAME=centos7.6.1810-gcc4.8.5-boost1.70-cmake3.15.6-gtest1.8.1-openssl.template

#openssl_major_versions=("1.0.0" "1.0.1" "1.0.2" "1.1.0" "1.1.1")
openssl_major_versions=("1.0.0")

# Minor versions (array names are transformed versions, example: "1.0.0" -> v100)
#v100=("" a b c d e f g h i j k l m n o p q r s t)
v100=("" a b c)
v101=("" a b c d e f g h i j k l m n o p q r s t u)
v102=("" a b c d e f g h i j k l m n o p q r s t u)
v110=("" a b c d e f g h i j k l)
v111=("" a b c d)

for i in "${openssl_major_versions[@]}" ; do
    i_transfromed=v$(echo $i | tr -d '.')
    var=$i_transfromed[@] # extracted as separate var for substitution with {!} construct
    for k in "${!var}" ; do
       echo $i$k
       ${SCRIPT_DIR}/build_single_openssl_versions.bash "${TEMPLATE_FILENAME}" "$i$k"
    done
done