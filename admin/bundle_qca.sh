#!/bin/sh

TARGET=third-party

################################################################################
# QCA
################################################################################

QCA_SOURCE=../qca
QCA_TARGET=third-party/qca

rm -rf $QCA_TARGET
cp -r $QCA_SOURCE/src $QCA_TARGET
cp -r $QCA_SOURCE/certs $QCA_TARGET
cp -r $QCA_SOURCE/include $QCA_TARGET
rm $QCA_TARGET/qt.tag $QCA_TARGET/src.pro
echo 'include(../qca.pri)' > $QCA_TARGET/qca.pro

################################################################################
# QCA-OpenSSL
################################################################################

QCAOPENSSL_SOURCE=../qca-openssl
QCAOPENSSL_TARGET=third-party/qca-openssl
rm -rf $QCAOPENSSL_TARGET
cp -r $QCAOPENSSL_SOURCE $QCA_OPENSSL_TARGET
