#!/usr/bin/awk -f
BEGIN {
  FS="^";
  print "[Substitutions]"
}

$3 !~ /^\\[[:alnum:]]+{\\/ && $3 ~ /^\\[[:alnum:]]+/ && !/^#/  {
  code=$1
  name=$3
  sub(/^\\/, "", name)
  sub(/^0*/, "", code)
  print name "=U+" code
}
