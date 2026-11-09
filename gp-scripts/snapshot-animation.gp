n = int(system("grep -c '^# time_s' output.snapshots.dat"))
set yrange [0:1e18]
do for [k=0:n-1:10] { plot 'output.snapshots.dat' every :::k::k u 1:3 w l t sprintf('J+  (snapshot %d)', k), '' every :::k::k u 1:4 w l t 'J-', '' every :::k::k u 1:2 w l t 'Inversion'; pause 0.1 }