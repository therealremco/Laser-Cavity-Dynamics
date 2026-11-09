set multiplot layout 3,1
set format y '%.1e'
plot 'output.dat' u 1:2 w l t 'output photons'
plot 'output.dat' u 1:5 w l t 'total inversion'
set xlabel 'time (s)'
plot 'output.dat' u 1:6 w l t 'pump energy (J per step)'
unset multiplot