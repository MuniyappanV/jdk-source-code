#! /bin/sh
# @(#)genBuffer.sh	1.11 05/01/26
# Generate concrete buffer classes

# Required environment variables
#   NAWK SED SPP    To invoke tools
#   TYPE            Primitive type
#   SRC             Source file
#   DST             Destination file
#
# Optional environment variables
#   RW              Mutability: R(ead only), W(ritable)
#   BO              Byte order: B(ig), L(ittle), S(wapped), U(nswapped)
#   BIN             Defined => generate binary-data access methods

type=$TYPE
rw=$RW
rwkey=XX

case $type in
  char)  fulltype=character;;
  *)     fulltype=$type;;
esac

case $type in
  byte)           LBPV=0;;
  char | short)   LBPV=1;;
  int | float)    LBPV=2;;
  long | double)  LBPV=3;;
esac

typesAndBits() {

  type="$1"; BO="$2"
  memtype=$type; swaptype=$type; frombits=; tobits=

  case $type in
    float)   memtype=int
             if [ x$BO != xU ]; then
	       swaptype=int
	       fromBits=Float.intBitsToFloat
	       toBits=Float.floatToRawIntBits
	     fi;;
    double)  memtype=long
             if [ x$BO != xU ]; then
	       swaptype=long
	       fromBits=Double.longBitsToDouble
	       toBits=Double.doubleToRawLongBits
	     fi;;
  esac

  echo memtype=$memtype swaptype=$swaptype fromBits=$fromBits toBits=$toBits

  echo $type $fulltype $memtype $swaptype \
  | $NAWK '{ type = $1; fulltype = $2; memtype = $3; swaptype = $4;
	     x = substr(type, 1, 1);
	     Type = toupper(x) substr(type, 2);
	     Fulltype = toupper(x) substr(fulltype, 2);
	     Memtype = toupper(substr(memtype, 1, 1)) substr(memtype, 2);
	     Swaptype = toupper(substr(swaptype, 1, 1)) substr(swaptype, 2);
	     printf("Type=%s x=%s Fulltype=%s Memtype=%s Swaptype=%s ",
		    Type, x, Fulltype, Memtype, Swaptype); }'

  echo "swap=`if [ x$BO = xS ]; then echo Bits.swap; fi`"

}

eval `typesAndBits $type $BO`

a=`if [ $type = int ]; then echo an; else echo a; fi`
A=`if [ $type = int ]; then echo An; else echo A; fi`

if [ "x$rw" = xR ]; then rwkey=ro; else rwkey=rw; fi

set -e

$SPP <$SRC >$DST \
  -K$type \
  -Dtype=$type \
  -DType=$Type \
  -Dfulltype=$fulltype \
  -DFulltype=$Fulltype \
  -Dx=$x \
  -Dmemtype=$memtype \
  -DMemtype=$Memtype \
  -DSwaptype=$Swaptype \
  -DfromBits=$fromBits \
  -DtoBits=$toBits \
  -DLG_BYTES_PER_VALUE=$LBPV \
  -DBYTES_PER_VALUE="(1 << $LBPV)" \
  -DBO=$BO \
  -Dswap=$swap \
  -DRW=$rw \
  -K$rwkey \
  -Da=$a \
  -DA=$A \
  -Kbo$BO

if [ $BIN ]; then

  genBinOps() {
    type="$1"
    Type=`echo $1 | $NAWK '{ print toupper(substr($1, 1, 1)) substr($1, 2) }'`
    fulltype="$2"
    LBPV="$3"
    nbytes="$4"
    nbytesButOne="$5"
    a=`if [ $type = int ]; then echo an; else echo a; fi`
    src=$6
    eval `typesAndBits $type`
    $SPP <$src \
      -Dtype=$type \
      -DType=$Type \
      -Dfulltype=$fulltype \
      -Dmemtype=$memtype \
      -DMemtype=$Memtype \
      -DfromBits=$fromBits \
      -DtoBits=$toBits \
      -DLG_BYTES_PER_VALUE=$LBPV \
      -DBYTES_PER_VALUE="(1 << $LBPV)" \
      -Dnbytes=$nbytes \
      -DnbytesButOne=$nbytesButOne \
      -DRW=$rw \
      -K$rwkey \
      -Da=$a \
      -be
  }

  mv $DST $DST.tmp
  sed -e '/#BIN/,$d' <$DST.tmp >$DST
  rm -f $DST.tmp
  binops=`dirname $SRC`/`basename $SRC .java`-bin.java
  genBinOps char character 1 two one $binops >>$DST
  genBinOps short short 1 two one $binops >>$DST
  genBinOps int integer 2 four three $binops >>$DST
  genBinOps long long 3 eight seven $binops >>$DST
  genBinOps float float 2 four three $binops >>$DST
  genBinOps double double 3 eight seven $binops >>$DST
  echo '}' >>$DST

fi
