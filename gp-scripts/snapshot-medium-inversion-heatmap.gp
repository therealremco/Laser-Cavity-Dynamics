dt = real(system("awk '/^# time_s = /{print $4; exit}' output.snapshots.dat"))
set view map
set xrange [0.005:0.01]
set yrange [1e-10:6e-10]
set logscale cb
set cbrange [1e19:3e22]
set xlabel 'z (m)'
set ylabel 'time (s)'
set cblabel 'N (m^{-3})'
splot 'output.snapshots.dat' u 1:((column(-1)+1)*dt):2 w pm3d t ''