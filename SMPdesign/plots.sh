#! /bin/sh

fontsize=10
plotsize=0.5

gnuplot << ---EOF---
set term pbm medium
set output "clockfreq.pbm"
set xlabel "Year"
set ylabel "CPU Clock Frequency"
set logscale y
#set yrange [1:10000]
set yrange [100:10000]
set nokey
# set label 1 "rcu" at 0.1,10 left
# set label 2 "spinlock" at 0.5,3.0 left
# set label 3 "brlock" at 0.4,0.6 left
# set label 4 "rwlock" at 0.3,1.6 left
# set label 5 "refcnt" at 0.15,2.8 left
#plot "clockfreq.dat", "clockfreqP4.dat", "clockfreqP3.dat"
plot "clockfreqP4.dat", "clockfreqP3.dat", "clockfreqP2.dat", "clockfreqPPro.dat", "clockfreqXeonDC.dat"
# plot "clockfreqP4.dat", "clockfreqP3.dat", "clockfreqP2.dat"
set term postscript portrait ${fontsize}
set size square ${plotsize},${plotsize}
set output "|../utilities/gnuplotepsfixdc > clockfreq.eps"
replot
---EOF---
ppmtogif clockfreq.pbm > clockfreq.gif 2> /dev/null