#!/bin/sh

echo 'Generating necessary files...'
libtoolize --copy --force
aclocal
autoheader
automake --gnu --add-missing
autoconf
