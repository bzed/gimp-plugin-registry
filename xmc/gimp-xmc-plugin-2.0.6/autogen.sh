#!/bin/bash
autoheader && aclocal && autoconf && automake -a -c && ./configure "$@"
