#!/usr/bin/bash
# Suites: cases
# 1. fifo-fn-fix: fifo-fn-fix
# 2. fifo-tlq-fix: fifo-tlq-fix-delwait fifo-tlq-fix-deltry
# 3. fifo-lfi: fifo-lfi
# 4. fifo-tlqi: fifo-tlqi-deltry
# 5. fifo-li: fifo-li-deltry

#set "CK_RUN_CASE="
#set "CK_RUN_SUITE="
#set "CK_FORK=no"
TP=./build/test/test-fifo

CK_RUN_CASE=fifo-tlq-fix-deltry ${TP} -l 4 $@
CK_RUN_SUITE=fifo-lfi ${TP} -l 4 $@
CK_RUN_CASE=fifo-tlqi-deltry ${TP} -l 4 $@
CK_RUN_CASE=fifo-li-deltry ${TP} -l 4 $@

