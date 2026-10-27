#!/bin/sh
# bench.sh <cfr_measure> [headline]
# Segment "train" seconds sum to the run. headline: 5 seeds, 16 threads, 409.6M.
set -eu
m=$1
d=$(mktemp -d)
trap 'rm -rf "$d"' EXIT INT TERM

train_s() {
  "$m" --train "$1" "$d/ck" "$d/lg" "$2" "$3" >"$d/out"
  awk '$5=="train"{s+=$6;n++} END{if(!n)exit 1; printf "%.6f\n",s}' "$d/out"
}

msd() {
  awk '{x[n++]=$1;s+=$1} END{m=s/n;v=0; for(i=0;i<n;i++)v+=(x[i]-m)^2
    printf "%.2f %.2f\n",m,(n>1)?sqrt(v/(n-1)):0}' "$1"
}

if [ "${2:-}" = headline ]; then
  : >"$d/rows"
  : >"$d/times"
  for seed in 1 2 3 4 5; do
    "$m" --train 409600000 "$d/ck" "$d/lg" "$seed" 16 >"$d/out"
    awk '{gsub(/,/,"",$4); print $1,$4}' "$d/out" >>"$d/rows"
    awk '$5=="train"{s+=$6} END{print s}' "$d/out" >>"$d/times"
  done
  awk '{k=$1; e[k,c[k]++]=$2; s[k]+=$2}
    END{for(k in s){n=c[k]; m=s[k]/n; v=0
      for(i=0;i<n;i++) v+=(e[k,i]-m)^2
      printf "%s  %.4f ± %.4f\n",k,m,(n>1)?sqrt(v/(n-1)):0}}' \
    "$d/rows" >"$d/sum"
  echo "iters  exploitability"
  sort -n "$d/sum"
  stats=$(msd "$d/times")
  echo "training  ${stats% *} ± ${stats#* } s"
  exit 0
fi

"$m" --partition
echo
echo "throughput  --train 1600000  seed 1  threads 1  n=5"
: >"$d/times"
for _ in 1 2 3 4 5; do train_s 1600000 1 1 >>"$d/times"; done
stats=$(msd "$d/times")
echo "${stats% *} ± ${stats#* } s"
echo
echo "scaling  --train 6400000  seed 1  n=3  (sum of train segments)"
echo "threads  train_s       speedup"
base=
for t in 1 2 4 8 16; do
  : >"$d/times"
  for _ in 1 2 3; do train_s 6400000 1 "$t" >>"$d/times"; done
  stats=$(msd "$d/times")
  mean=${stats% *}
  sd=${stats#* }
  [ -n "$base" ] || base=$mean
  speed=$(awk -v b="$base" -v x="$mean" 'BEGIN{printf "%.2f", b/x}')
  printf '%7s  %s ± %s  %sx\n' "$t" "$mean" "$sd" "$speed"
done
