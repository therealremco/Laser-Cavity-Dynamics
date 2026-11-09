dt = real(system("awk '/^# time_s = /{print $4; exit}' output.snapshots.dat"))
set view map
set xlabel 'z (m)'
set ylabel 'time (s)'
splot 'output.snapshots.dat' u 1:((column(-1)+1)*dt):2 w pm3d t 'N'